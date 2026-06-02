#include "IcdImportDialog.h"

#include "FieldCsvCodec.h"
#include "IcdDocxImporter.h"
#include "InputValidator.h"
#include "Themes.h"

#include <QAbstractItemView>
#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace
{
QString elide(const QString& s, int n)
{
    const QString t = s.trimmed();
    if (t.size() <= n)
        return t;
    return t.left(n - 3) + "...";
}

// First column whose (lower-cased) header text contains any keyword, else -1.
int matchColumn(const QStringList& headers, const QStringList& keywords)
{
    for (int i = 0; i < headers.size(); ++i)
    {
        const QString low = headers.at(i).trimmed().toLower();
        if (low.isEmpty())
            continue;
        for (int k = 0; k < keywords.size(); ++k)
            if (low.contains(keywords.at(k)))
                return i;
    }
    return -1;
}

// Rebuild a role combo's items as "(not mapped)" + one entry per column. Preserves
// a prior explicit column choice; on first fill, preselects the best keyword match.
void fillRoleCombo(QComboBox* combo, const QStringList& headers, const QStringList& keywords)
{
    const int prev = (combo->count() > 0) ? combo->currentData().toInt() : -2;

    combo->blockSignals(true);
    combo->clear();
    combo->addItem("(not mapped)", -1);
    for (int i = 0; i < headers.size(); ++i)
    {
        QString h = headers.at(i).trimmed();
        if (h.isEmpty())
            h = "(blank)";
        combo->addItem(QString("Col %1: %2").arg(i).arg(elide(h, 28)), i);
    }

    int sel = -1;
    if (prev >= 0 && prev < headers.size())
        sel = prev;
    else if (prev == -2)
        sel = matchColumn(headers, keywords);

    const int idx = combo->findData(sel);
    combo->setCurrentIndex(idx >= 0 ? idx : 0);
    combo->blockSignals(false);
}

void setComboData(QComboBox* combo, int dataValue)
{
    const int idx = combo->findData(dataValue);
    if (idx >= 0)
        combo->setCurrentIndex(idx);
}

const int TREE_COL_ITEM = 0;
const int TREE_COL_PORT = 1;
const int TREE_COL_LEN = 2;
const int TREE_COL_HEADER = 3;
}

IcdImportDialog::IcdImportDialog(QWidget* parent)
    : QDialog(parent),
      m_lstTables(0),
      m_spnHeaderRow(0),
      m_cmbOffsetBase(0),
      m_lblColumnsFor(0),
      m_cmbColName(0),
      m_cmbColOffset(0),
      m_cmbColType(0),
      m_cmbColLength(0),
      m_cmbColResolution(0),
      m_cmbColExpr(0),
      m_cmbNameSource(0),
      m_txtNamePrefix(0),
      m_spnDefaultPort(0),
      m_chkAutoLength(0),
      m_btnAutoDetect(0),
      m_tree(0),
      m_txtWarnings(0),
      m_autoDetectedForTable(-2)
{
    buildUi();
    Themes::apply(this);
}

void IcdImportDialog::buildUi()
{
    setWindowTitle("Import ICD (Word .docx)");
    resize(880, 780);

    QVBoxLayout* root = new QVBoxLayout(this);

    // ---- 1. Tables ----
    QGroupBox* gTables = new QGroupBox("1. Tables found in the document", this);
    QVBoxLayout* lTables = new QVBoxLayout(gTables);
    QLabel* hint = new QLabel(
        "Tick the tables that contain field definitions. Click a table to use its "
        "columns for the mapping below.", gTables);
    hint->setWordWrap(true);
    m_lstTables = new QListWidget(gTables);
    m_lstTables->setMaximumHeight(150);
    lTables->addWidget(hint);
    lTables->addWidget(m_lstTables);
    root->addWidget(gTables);

    // ---- 2. Column mapping ----
    QGroupBox* gMap = new QGroupBox("2. Column mapping (applies to all ticked tables)", this);
    QGridLayout* lMap = new QGridLayout(gMap);
    int r = 0;
    lMap->addWidget(new QLabel("Header row index:", gMap), r, 0);
    m_spnHeaderRow = new QSpinBox(gMap);
    m_spnHeaderRow->setRange(0, 999);
    lMap->addWidget(m_spnHeaderRow, r, 1);
    lMap->addWidget(new QLabel("Offset base:", gMap), r, 2);
    m_cmbOffsetBase = new QComboBox(gMap);
    m_cmbOffsetBase->addItem("0-based (offset 0 = first byte)");
    m_cmbOffsetBase->addItem("1-based (offset 1 = first byte)");
    lMap->addWidget(m_cmbOffsetBase, r, 3);
    ++r;

    m_lblColumnsFor = new QLabel("Columns: (no table selected)", gMap);
    m_lblColumnsFor->setWordWrap(true);
    lMap->addWidget(m_lblColumnsFor, r, 0, 1, 4);
    ++r;

    lMap->addWidget(new QLabel("Name column *:", gMap), r, 0);
    m_cmbColName = new QComboBox(gMap);
    lMap->addWidget(m_cmbColName, r, 1);
    lMap->addWidget(new QLabel("ByteOffset column *:", gMap), r, 2);
    m_cmbColOffset = new QComboBox(gMap);
    lMap->addWidget(m_cmbColOffset, r, 3);
    ++r;
    lMap->addWidget(new QLabel("DataType column *:", gMap), r, 0);
    m_cmbColType = new QComboBox(gMap);
    lMap->addWidget(m_cmbColType, r, 1);
    lMap->addWidget(new QLabel("Length column:", gMap), r, 2);
    m_cmbColLength = new QComboBox(gMap);
    lMap->addWidget(m_cmbColLength, r, 3);
    ++r;
    lMap->addWidget(new QLabel("Resolution column:", gMap), r, 0);
    m_cmbColResolution = new QComboBox(gMap);
    lMap->addWidget(m_cmbColResolution, r, 1);
    lMap->addWidget(new QLabel("Resolution expr column:", gMap), r, 2);
    m_cmbColExpr = new QComboBox(gMap);
    lMap->addWidget(m_cmbColExpr, r, 3);
    ++r;
    m_btnAutoDetect = new QPushButton("Auto-detect columns from table contents", gMap);
    m_btnAutoDetect->setToolTip(
        "Inspect the selected table's data and guess the header row, offset base and "
        "column roles — column order does not matter. Runs automatically when you click a "
        "table; click here to re-run (e.g. after changing the header row). Always review "
        "the result before importing.");
    lMap->addWidget(m_btnAutoDetect, r, 0, 1, 4);
    ++r;
    root->addWidget(gMap);

    // ---- 3. Message identity ----
    QGroupBox* gMsg = new QGroupBox("3. Message identity", this);
    QGridLayout* lMsg = new QGridLayout(gMsg);
    int m = 0;
    lMsg->addWidget(new QLabel("Message name from:", gMsg), m, 0);
    m_cmbNameSource = new QComboBox(gMsg);
    m_cmbNameSource->addItem("Heading above each table");
    m_cmbNameSource->addItem("Custom prefix + number");
    lMsg->addWidget(m_cmbNameSource, m, 1);
    lMsg->addWidget(new QLabel("Custom prefix:", gMsg), m, 2);
    m_txtNamePrefix = new QLineEdit("Message", gMsg);
    lMsg->addWidget(m_txtNamePrefix, m, 3);
    ++m;
    lMsg->addWidget(new QLabel("Default port:", gMsg), m, 0);
    m_spnDefaultPort = new QSpinBox(gMsg);
    m_spnDefaultPort->setRange(1, 65535);
    m_spnDefaultPort->setValue(5000);
    lMsg->addWidget(m_spnDefaultPort, m, 1);
    m_chkAutoLength = new QCheckBox("Auto payload length from field extents", gMsg);
    m_chkAutoLength->setChecked(true);
    lMsg->addWidget(m_chkAutoLength, m, 2, 1, 2);
    ++m;
    root->addWidget(gMsg);

    // ---- build / profile buttons ----
    QHBoxLayout* lBtns = new QHBoxLayout();
    QPushButton* btnBuild = new QPushButton("Build / Preview", this);
    QPushButton* btnSaveP = new QPushButton("Save Mapping...", this);
    QPushButton* btnLoadP = new QPushButton("Load Mapping...", this);
    lBtns->addWidget(btnBuild);
    lBtns->addStretch();
    lBtns->addWidget(btnLoadP);
    lBtns->addWidget(btnSaveP);
    root->addLayout(lBtns);

    // ---- 4. Review tree ----
    QGroupBox* gRev = new QGroupBox(
        "4. Review & select (tick what to import; double-click a message row to edit "
        "its port / length / header)", this);
    QVBoxLayout* lRev = new QVBoxLayout(gRev);
    QHBoxLayout* lCheck = new QHBoxLayout();
    QPushButton* btnAll = new QPushButton("Check All", gRev);
    QPushButton* btnNone = new QPushButton("Uncheck All", gRev);
    lCheck->addWidget(btnAll);
    lCheck->addWidget(btnNone);
    lCheck->addStretch();
    lRev->addLayout(lCheck);
    m_tree = new QTreeWidget(gRev);
    m_tree->setColumnCount(4);
    QStringList treeHeaders;
    treeHeaders << "Message / Field" << "Port" << "Payload Len" << "Optional Header (hex)";
    m_tree->setHeaderLabels(treeHeaders);
    m_tree->setEditTriggers(QAbstractItemView::DoubleClicked
                            | QAbstractItemView::SelectedClicked
                            | QAbstractItemView::EditKeyPressed);
    lRev->addWidget(m_tree);
    root->addWidget(gRev, 1);

    // ---- warnings ----
    root->addWidget(new QLabel("Warnings:", this));
    m_txtWarnings = new QPlainTextEdit(this);
    m_txtWarnings->setReadOnly(true);
    m_txtWarnings->setMaximumHeight(110);
    m_txtWarnings->setPlaceholderText("Parser warnings will appear here after Build / Preview.");
    root->addWidget(m_txtWarnings);

    // ---- dialog buttons ----
    QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(bb);

    connect(m_lstTables, SIGNAL(currentRowChanged(int)), this, SLOT(onReferenceTableChanged()));
    connect(m_spnHeaderRow, SIGNAL(valueChanged(int)), this, SLOT(onReferenceTableChanged()));
    connect(m_btnAutoDetect, SIGNAL(clicked()), this, SLOT(onAutoDetectClicked()));
    connect(m_cmbNameSource, SIGNAL(currentIndexChanged(int)), this, SLOT(onNameSourceChanged()));
    connect(btnBuild, SIGNAL(clicked()), this, SLOT(onBuildClicked()));
    connect(btnSaveP, SIGNAL(clicked()), this, SLOT(onSaveProfileClicked()));
    connect(btnLoadP, SIGNAL(clicked()), this, SLOT(onLoadProfileClicked()));
    connect(btnAll, SIGNAL(clicked()), this, SLOT(onCheckAll()));
    connect(btnNone, SIGNAL(clicked()), this, SLOT(onUncheckAll()));
    connect(bb, SIGNAL(accepted()), this, SLOT(onAccept()));
    connect(bb, SIGNAL(rejected()), this, SLOT(reject()));

    onNameSourceChanged();
}

void IcdImportDialog::setDocument(const IcdDocument& doc)
{
    m_doc = doc;
    m_drafts.clear();
    m_result.clear();
    if (m_tree)
        m_tree->clear();
    if (m_txtWarnings)
        m_txtWarnings->clear();
    populateTableList();
    if (m_lstTables->count() > 0)
        m_lstTables->setCurrentRow(0);
    else
        repopulateColumnCombos();
}

void IcdImportDialog::populateTableList()
{
    m_lstTables->clear();
    for (int i = 0; i < m_doc.tables.size(); ++i)
    {
        const IcdRawTable& t = m_doc.tables.at(i);
        QString heading = t.precedingHeading.trimmed();
        if (heading.isEmpty())
            heading = "(no heading)";
        QListWidgetItem* item = new QListWidgetItem(
            QString("Table %1: %2   [%3 rows x %4 cols]")
                .arg(i + 1).arg(elide(heading, 60)).arg(t.rows.size()).arg(t.columnCount),
            m_lstTables);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        // Pre-tick likely field tables; the user reviews and overrides.
        const bool likelyFieldTable = (t.columnCount >= 3 && t.rows.size() >= 2);
        item->setCheckState(likelyFieldTable ? Qt::Checked : Qt::Unchecked);
    }
}

int IcdImportDialog::referenceTableIndex() const
{
    if (!m_lstTables)
        return -1;
    const int cur = m_lstTables->currentRow();
    if (cur >= 0 && cur < m_doc.tables.size())
        return cur;
    for (int i = 0; i < m_lstTables->count() && i < m_doc.tables.size(); ++i)
    {
        QListWidgetItem* it = m_lstTables->item(i);
        if (it && it->checkState() == Qt::Checked)
            return i;
    }
    return m_doc.tables.isEmpty() ? -1 : 0;
}

QStringList IcdImportDialog::referenceHeaderCells() const
{
    const int ref = referenceTableIndex();
    if (ref < 0 || ref >= m_doc.tables.size())
        return QStringList();
    const IcdRawTable& t = m_doc.tables.at(ref);
    if (t.rows.isEmpty())
        return QStringList();
    int hr = m_spnHeaderRow ? m_spnHeaderRow->value() : 0;
    if (hr < 0)
        hr = 0;
    if (hr >= t.rows.size())
        hr = t.rows.size() - 1;
    return t.rows.at(hr);
}

void IcdImportDialog::repopulateColumnCombos()
{
    const QStringList headers = referenceHeaderCells();
    const int ref = referenceTableIndex();
    if (ref < 0)
    {
        m_lblColumnsFor->setText("Columns: (no table selected)");
    }
    else
    {
        QString hd = m_doc.tables.at(ref).precedingHeading.trimmed();
        if (hd.isEmpty())
            hd = "(no heading)";
        QStringList preview;
        for (int i = 0; i < headers.size() && i < 12; ++i)
        {
            const QString h = headers.at(i).trimmed();
            preview << QString("[%1] %2").arg(i).arg(h.isEmpty() ? QString("(blank)") : h);
        }
        m_lblColumnsFor->setText(QString("Columns for Table %1 - %2:   %3")
                                     .arg(ref + 1).arg(elide(hd, 40)).arg(preview.join("   ")));
    }

    fillRoleCombo(m_cmbColName, headers, QStringList() << "name" << "field" << "parameter" << "signal" << "mnemonic");
    fillRoleCombo(m_cmbColOffset, headers, QStringList() << "offset" << "position");
    fillRoleCombo(m_cmbColType, headers, QStringList() << "type" << "format" << "encoding");
    fillRoleCombo(m_cmbColLength, headers, QStringList() << "length" << "len" << "size" << "width" << "bytes" << "octet");
    fillRoleCombo(m_cmbColResolution, headers, QStringList() << "resolution" << "scale" << "lsb" << "factor");
    fillRoleCombo(m_cmbColExpr, headers, QStringList() << "expression" << "formula" << "conversion" << "equation" << "scaling");
}

void IcdImportDialog::onReferenceTableChanged()
{
    // When the reference *table* changes, auto-detect its mapping. When only the
    // header-row spin changed (same table), just refill the combos so the user's
    // manual header-row choice is respected.
    const int ref = referenceTableIndex();
    if (ref >= 0 && ref != m_autoDetectedForTable)
    {
        m_autoDetectedForTable = ref;
        autoDetectMapping(ref);
        return;
    }
    repopulateColumnCombos();
}

void IcdImportDialog::onAutoDetectClicked()
{
    const int ref = referenceTableIndex();
    if (ref < 0)
    {
        QMessageBox::information(this, "Auto-detect",
            "Select a table first (click a row in the list above).");
        return;
    }
    m_autoDetectedForTable = ref;
    autoDetectMapping(ref);
}

void IcdImportDialog::autoDetectMapping(int tableIndex)
{
    if (tableIndex < 0 || tableIndex >= m_doc.tables.size())
    {
        repopulateColumnCombos();
        return;
    }

    // Keep the user's non-column settings (name source, port, auto-length); let the
    // heuristic fill header row, offset base and the role columns.
    IcdMappingProfile sug = currentProfileFromUi();
    IcdDocxImporter::suggestMapping(m_doc.tables.at(tableIndex), sug);

    m_spnHeaderRow->blockSignals(true);            // avoid re-entering this slot
    m_spnHeaderRow->setValue(sug.headerRowIndex);
    m_spnHeaderRow->blockSignals(false);
    m_cmbOffsetBase->setCurrentIndex(sug.offsetBase == 1 ? 1 : 0);

    // Keyword pre-fill for the detected header row, then override with the
    // content-based guesses where they are confident (>= 0).
    repopulateColumnCombos();
    if (sug.colName >= 0)           setComboData(m_cmbColName, sug.colName);
    if (sug.colByteOffset >= 0)     setComboData(m_cmbColOffset, sug.colByteOffset);
    if (sug.colDataType >= 0)       setComboData(m_cmbColType, sug.colDataType);
    if (sug.colLength >= 0)         setComboData(m_cmbColLength, sug.colLength);
    if (sug.colResolution >= 0)     setComboData(m_cmbColResolution, sug.colResolution);
    if (sug.colResolutionExpr >= 0) setComboData(m_cmbColExpr, sug.colResolutionExpr);
}

void IcdImportDialog::onNameSourceChanged()
{
    const bool custom = (m_cmbNameSource->currentIndex() == int(IcdNameSource::CustomPrefix));
    m_txtNamePrefix->setEnabled(custom);
}

IcdMappingProfile IcdImportDialog::currentProfileFromUi() const
{
    IcdMappingProfile p;
    p.headerRowIndex = m_spnHeaderRow->value();
    p.offsetBase = m_cmbOffsetBase->currentIndex();
    p.colName = m_cmbColName->currentData().toInt();
    p.colByteOffset = m_cmbColOffset->currentData().toInt();
    p.colDataType = m_cmbColType->currentData().toInt();
    p.colLength = m_cmbColLength->currentData().toInt();
    p.colResolution = m_cmbColResolution->currentData().toInt();
    p.colResolutionExpr = m_cmbColExpr->currentData().toInt();
    p.nameSource = m_cmbNameSource->currentIndex();
    p.customNamePrefix = m_txtNamePrefix->text().trimmed();
    if (p.customNamePrefix.isEmpty())
        p.customNamePrefix = "Message";
    p.defaultPort = m_spnDefaultPort->value();
    p.autoPayloadLength = m_chkAutoLength->isChecked();
    return p;
}

void IcdImportDialog::applyProfileToUi(const IcdMappingProfile& profile)
{
    m_spnHeaderRow->blockSignals(true);
    m_spnHeaderRow->setValue(profile.headerRowIndex);
    m_spnHeaderRow->blockSignals(false);
    m_cmbOffsetBase->setCurrentIndex(profile.offsetBase == 1 ? 1 : 0);
    m_cmbNameSource->setCurrentIndex(profile.nameSource == 1 ? 1 : 0);
    m_txtNamePrefix->setText(profile.customNamePrefix);
    int port = profile.defaultPort;
    if (port < 1) port = 1;
    if (port > 65535) port = 65535;
    m_spnDefaultPort->setValue(port);
    m_chkAutoLength->setChecked(profile.autoPayloadLength);

    repopulateColumnCombos();
    setComboData(m_cmbColName, profile.colName);
    setComboData(m_cmbColOffset, profile.colByteOffset);
    setComboData(m_cmbColType, profile.colDataType);
    setComboData(m_cmbColLength, profile.colLength);
    setComboData(m_cmbColResolution, profile.colResolution);
    setComboData(m_cmbColExpr, profile.colResolutionExpr);
    onNameSourceChanged();

    // A loaded mapping is authoritative for the current table — don't let auto-detect
    // immediately overwrite it. (Switching to a different table will still re-detect.)
    m_autoDetectedForTable = referenceTableIndex();
}

QList<int> IcdImportDialog::checkedTableIndices() const
{
    QList<int> out;
    for (int i = 0; i < m_lstTables->count() && i < m_doc.tables.size(); ++i)
    {
        QListWidgetItem* it = m_lstTables->item(i);
        if (it && it->checkState() == Qt::Checked)
            out << i;
    }
    return out;
}

void IcdImportDialog::onBuildClicked()
{
    const IcdMappingProfile profile = currentProfileFromUi();
    if (profile.colName < 0 || profile.colByteOffset < 0 || profile.colDataType < 0)
    {
        QMessageBox::warning(this, "Import ICD",
            "Map the Name, ByteOffset and DataType columns first (the three marked with *).");
        return;
    }
    const QList<int> selected = checkedTableIndices();
    if (selected.isEmpty())
    {
        QMessageBox::warning(this, "Import ICD", "Tick at least one table to import.");
        return;
    }

    QStringList globalWarnings;
    IcdDocxImporter::buildDrafts(m_doc, selected, profile, m_drafts, globalWarnings);
    populateReviewTree();

    QStringList all = globalWarnings;
    for (int i = 0; i < m_drafts.size(); ++i)
        all << m_drafts.at(i).warnings;
    m_txtWarnings->setPlainText(all.isEmpty()
        ? QString("No warnings. Review the messages/fields below and click OK.")
        : all.join("\n"));
}

void IcdImportDialog::populateReviewTree()
{
    m_tree->clear();
    for (int di = 0; di < m_drafts.size(); ++di)
    {
        const MessageDefinition& msg = m_drafts.at(di).message;
        QTreeWidgetItem* mi = new QTreeWidgetItem(m_tree);
        mi->setText(TREE_COL_ITEM, msg.messageName);
        mi->setText(TREE_COL_PORT, QString::number(msg.port));
        mi->setText(TREE_COL_LEN, QString::number(msg.payloadLengthBytes));
        mi->setText(TREE_COL_HEADER, msg.optionalHeader.isEmpty()
            ? QString()
            : QString::fromLatin1(msg.optionalHeader.toHex()).toUpper());
        mi->setFlags(mi->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
        mi->setCheckState(TREE_COL_ITEM, msg.fields.isEmpty() ? Qt::Unchecked : Qt::Checked);
        mi->setData(TREE_COL_ITEM, Qt::UserRole, di);

        for (int fi = 0; fi < msg.fields.size(); ++fi)
        {
            const FieldDefinition& f = msg.fields.at(fi);
            QTreeWidgetItem* ci = new QTreeWidgetItem(mi);
            const QString disp = QString("%1     [offset %2, %3, len %4, res %5]")
                .arg(f.name)
                .arg(f.byteOffset)
                .arg(FieldCsvCodec::dataTypeToLabel(f.dataType))
                .arg(f.length)
                .arg(QString::number(f.resolution, 'g', 10));
            ci->setText(TREE_COL_ITEM, disp);
            ci->setFlags((ci->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
            ci->setCheckState(TREE_COL_ITEM, Qt::Checked);
            ci->setData(TREE_COL_ITEM, Qt::UserRole, fi);
        }
        mi->setExpanded(true);
    }
    m_tree->resizeColumnToContents(TREE_COL_ITEM);
}

void IcdImportDialog::onCheckAll()
{
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* mi = m_tree->topLevelItem(i);
        mi->setCheckState(TREE_COL_ITEM, Qt::Checked);
        for (int j = 0; j < mi->childCount(); ++j)
            mi->child(j)->setCheckState(TREE_COL_ITEM, Qt::Checked);
    }
}

void IcdImportDialog::onUncheckAll()
{
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* mi = m_tree->topLevelItem(i);
        mi->setCheckState(TREE_COL_ITEM, Qt::Unchecked);
        for (int j = 0; j < mi->childCount(); ++j)
            mi->child(j)->setCheckState(TREE_COL_ITEM, Qt::Unchecked);
    }
}

void IcdImportDialog::onSaveProfileClicked()
{
    IcdMappingProfile profile = currentProfileFromUi();
    bool ok = false;
    const QString name = QInputDialog::getText(this, "Save Mapping",
        "Profile name:", QLineEdit::Normal,
        profile.profileName.isEmpty() ? QString("My ICD") : profile.profileName, &ok);
    if (!ok || name.trimmed().isEmpty())
        return;
    profile.profileName = name.trimmed();
    QString err;
    if (!IcdDocxImporter::saveProfile(profile, err))
        QMessageBox::warning(this, "Save Mapping", err);
    else
        QMessageBox::information(this, "Save Mapping",
            QString("Saved mapping '%1'.").arg(profile.profileName));
}

void IcdImportDialog::onLoadProfileClicked()
{
    const QStringList names = IcdDocxImporter::availableProfiles();
    if (names.isEmpty())
    {
        QMessageBox::information(this, "Load Mapping",
            "No saved mappings yet. Configure the mapping and use Save Mapping first.");
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getItem(this, "Load Mapping",
        "Choose a saved mapping:", names, 0, false, &ok);
    if (!ok || name.isEmpty())
        return;
    IcdMappingProfile p;
    QString err;
    if (!IcdDocxImporter::loadProfile(name, p, err))
    {
        QMessageBox::warning(this, "Load Mapping", err);
        return;
    }
    applyProfileToUi(p);
}

void IcdImportDialog::onAccept()
{
    m_result.clear();
    QStringList errors;
    QSet<QString> usedNames;

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* mi = m_tree->topLevelItem(i);
        if (mi->checkState(TREE_COL_ITEM) != Qt::Checked)
            continue;

        const int di = mi->data(TREE_COL_ITEM, Qt::UserRole).toInt();
        if (di < 0 || di >= m_drafts.size())
            continue;
        const MessageDefinition& base = m_drafts.at(di).message;

        MessageDefinition msg;
        msg.dataFormat = "HEX";
        msg.messageName = mi->text(TREE_COL_ITEM).trimmed();
        if (msg.messageName.isEmpty())
        {
            errors << QString("Row %1: message name is empty.").arg(i + 1);
            continue;
        }
        if (usedNames.contains(msg.messageName))
        {
            errors << QString("Duplicate selected message name '%1'.").arg(msg.messageName);
            continue;
        }

        bool portOk = false;
        const int port = mi->text(TREE_COL_PORT).trimmed().toInt(&portOk);
        QString portErr;
        if (!portOk || !InputValidator::validatePortValue(port, portErr))
        {
            errors << QString("Message '%1': invalid port '%2'. %3")
                          .arg(msg.messageName).arg(mi->text(TREE_COL_PORT)).arg(portErr);
            continue;
        }
        msg.port = static_cast<quint16>(port);

        bool lenOk = false;
        const int len = mi->text(TREE_COL_LEN).trimmed().toInt(&lenOk);
        if (!lenOk || len < 0)
        {
            errors << QString("Message '%1': invalid payload length '%2'.")
                          .arg(msg.messageName).arg(mi->text(TREE_COL_LEN));
            continue;
        }
        msg.payloadLengthBytes = len;

        const QString htext = mi->text(TREE_COL_HEADER).trimmed();
        if (!htext.isEmpty())
        {
            QByteArray hb;
            QString hlabel, herr;
            if (!InputValidator::validateHeaderHexText(htext, hb, hlabel, herr))
            {
                errors << QString("Message '%1': invalid optional header '%2'. %3")
                              .arg(msg.messageName).arg(htext).arg(herr);
                continue;
            }
            msg.optionalHeader = hb;
        }

        QList<FieldDefinition> selectedFields;
        for (int j = 0; j < mi->childCount(); ++j)
        {
            QTreeWidgetItem* ci = mi->child(j);
            if (ci->checkState(TREE_COL_ITEM) != Qt::Checked)
                continue;
            const int fi = ci->data(TREE_COL_ITEM, Qt::UserRole).toInt();
            if (fi >= 0 && fi < base.fields.size())
                selectedFields << base.fields.at(fi);
        }
        if (selectedFields.isEmpty())
        {
            errors << QString("Message '%1': no fields are ticked.").arg(msg.messageName);
            continue;
        }
        msg.fields = selectedFields;

        QString fieldErr;
        if (!InputValidator::validateFields(msg.fields, fieldErr))
        {
            errors << QString("Message '%1': %2").arg(msg.messageName).arg(fieldErr);
            continue;
        }

        usedNames.insert(msg.messageName);
        m_result.append(msg);
    }

    if (!errors.isEmpty())
    {
        QMessageBox::warning(this, "Import ICD",
            QString("Please fix the following before importing:\n\n%1").arg(errors.join("\n")));
        return;
    }
    if (m_result.isEmpty())
    {
        QMessageBox::information(this, "Import ICD",
            "Nothing is ticked to import. Tick at least one message (with at least one field).");
        return;
    }
    QDialog::accept();
}

QList<MessageDefinition> IcdImportDialog::selectedMessages() const
{
    return m_result;
}

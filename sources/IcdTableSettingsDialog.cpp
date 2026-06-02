#include "IcdTableSettingsDialog.h"
#include "ui_IcdTableSettingsDialog.h"

#include "IcdDocxImporter.h"
#include "Themes.h"

#include <QComboBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>

namespace
{
QString elide(const QString& s, int n)
{
    const QString t = s.trimmed();
    if (t.size() <= n)
        return t;
    return t.left(n - 3) + "...";
}

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

// Rebuild a role combo as "(not mapped)" + one entry per column. Preserves a prior
// explicit choice; on first fill, preselects the best keyword match.
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
}

IcdTableSettingsDialog::IcdTableSettingsDialog(QWidget* parent)
    : QDialog(parent),
      m_tableIndex(-1),
      ui(new Ui::IcdTableSettingsDialog)
{
    ui->setupUi(this);
    Themes::apply(this);

    connect(ui->btnAutoDetect, SIGNAL(clicked()), this, SLOT(onAutoDetectClicked()));
    connect(ui->cmbNameSource, SIGNAL(currentIndexChanged(int)), this, SLOT(onNameSourceChanged()));
    connect(ui->spnHeaderRow, SIGNAL(valueChanged(int)), this, SLOT(fillCombosForTable()));
    connect(ui->btnUnmergeAll, SIGNAL(clicked()), this, SLOT(onUnmergeAllClicked()));
    connect(ui->btnSaveMapping, SIGNAL(clicked()), this, SLOT(onSaveProfileClicked()));
    connect(ui->btnLoadMapping, SIGNAL(clicked()), this, SLOT(onLoadProfileClicked()));
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

IcdTableSettingsDialog::~IcdTableSettingsDialog()
{
    delete ui;
}

void IcdTableSettingsDialog::setContext(const IcdDocument& doc, int tableIndex,
                                        const IcdMappingProfile& mapping,
                                        const QList<int>& candidateTables,
                                        const QStringList& candidateLabels,
                                        const QList<int>& currentChildren)
{
    m_doc = doc;
    m_tableIndex = tableIndex;
    m_candidateTables = candidateTables;

    QString heading;
    if (tableIndex >= 0 && tableIndex < doc.tables.size())
    {
        heading = doc.tables.at(tableIndex).precedingHeading.trimmed();
        if (heading.isEmpty())
            heading = "(no heading)";
        ui->lblTableTitle->setText(QString("Table %1 - %2   [%3 rows x %4 cols]")
                                       .arg(tableIndex + 1).arg(elide(heading, 60))
                                       .arg(doc.tables.at(tableIndex).rows.size())
                                       .arg(doc.tables.at(tableIndex).columnCount));
    }
    else
    {
        ui->lblTableTitle->setText("Table");
    }

    applyMappingToUi(mapping);

    // Populate the join candidates (other selected tables), pre-ticking children.
    ui->lstJoin->clear();
    for (int i = 0; i < m_candidateTables.size(); ++i)
    {
        const int tIdx = m_candidateTables.at(i);
        const QString label = (i < candidateLabels.size())
                                  ? candidateLabels.at(i)
                                  : QString("Table %1").arg(tIdx + 1);
        QListWidgetItem* item = new QListWidgetItem(label, ui->lstJoin);
        item->setData(Qt::UserRole, tIdx);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(currentChildren.contains(tIdx) ? Qt::Checked : Qt::Unchecked);
    }
    ui->gJoin->setEnabled(!m_candidateTables.isEmpty());
}

QStringList IcdTableSettingsDialog::headerCells() const
{
    if (m_tableIndex < 0 || m_tableIndex >= m_doc.tables.size())
        return QStringList();
    const IcdRawTable& t = m_doc.tables.at(m_tableIndex);
    if (t.rows.isEmpty())
        return QStringList();
    int hr = ui->spnHeaderRow->value();
    if (hr < 0)
        hr = 0;
    if (hr >= t.rows.size())
        hr = t.rows.size() - 1;
    return t.rows.at(hr);
}

void IcdTableSettingsDialog::fillCombosForTable()
{
    const QStringList headers = headerCells();

    QStringList preview;
    for (int i = 0; i < headers.size() && i < 12; ++i)
    {
        const QString h = headers.at(i).trimmed();
        preview << QString("[%1] %2").arg(i).arg(h.isEmpty() ? QString("(blank)") : h);
    }
    ui->lblColumns->setText(preview.isEmpty()
        ? QString("Columns: (none)")
        : QString("Columns:   %1").arg(preview.join("   ")));

    fillRoleCombo(ui->cmbColName, headers, QStringList() << "name" << "field" << "parameter" << "signal" << "mnemonic");
    fillRoleCombo(ui->cmbColOffset, headers, QStringList() << "offset" << "position");
    fillRoleCombo(ui->cmbColType, headers, QStringList() << "type" << "format" << "encoding");
    fillRoleCombo(ui->cmbColLength, headers, QStringList() << "length" << "len" << "size" << "width" << "bytes" << "octet");
    fillRoleCombo(ui->cmbColResolution, headers, QStringList() << "resolution" << "scale" << "lsb" << "factor");
    fillRoleCombo(ui->cmbColExpr, headers, QStringList() << "expression" << "formula" << "conversion" << "equation" << "scaling");
}

void IcdTableSettingsDialog::applyMappingToUi(const IcdMappingProfile& m)
{
    ui->spnHeaderRow->blockSignals(true);
    ui->spnHeaderRow->setValue(m.headerRowIndex);
    ui->spnHeaderRow->blockSignals(false);
    ui->cmbOffsetBase->setCurrentIndex(m.offsetBase == 1 ? 1 : 0);
    ui->cmbNameSource->setCurrentIndex(m.nameSource == int(IcdNameSource::CustomPrefix) ? 1 : 0);
    ui->txtName->setText(m.customNamePrefix == "Message" ? QString() : m.customNamePrefix);
    int port = m.defaultPort;
    if (port < 1) port = 1;
    if (port > 65535) port = 65535;
    ui->spnPort->setValue(port);
    ui->chkAutoLength->setChecked(m.autoPayloadLength);

    fillCombosForTable();
    setComboData(ui->cmbColName, m.colName);
    setComboData(ui->cmbColOffset, m.colByteOffset);
    setComboData(ui->cmbColType, m.colDataType);
    setComboData(ui->cmbColLength, m.colLength);
    setComboData(ui->cmbColResolution, m.colResolution);
    setComboData(ui->cmbColExpr, m.colResolutionExpr);
    onNameSourceChanged();
}

IcdMappingProfile IcdTableSettingsDialog::collectMappingFromUi() const
{
    IcdMappingProfile p;
    p.headerRowIndex = ui->spnHeaderRow->value();
    p.offsetBase = ui->cmbOffsetBase->currentIndex();
    p.colName = ui->cmbColName->currentData().toInt();
    p.colByteOffset = ui->cmbColOffset->currentData().toInt();
    p.colDataType = ui->cmbColType->currentData().toInt();
    p.colLength = ui->cmbColLength->currentData().toInt();
    p.colResolution = ui->cmbColResolution->currentData().toInt();
    p.colResolutionExpr = ui->cmbColExpr->currentData().toInt();
    p.nameSource = ui->cmbNameSource->currentIndex();
    p.customNamePrefix = ui->txtName->text().trimmed();
    p.defaultPort = ui->spnPort->value();
    p.autoPayloadLength = ui->chkAutoLength->isChecked();
    return p;
}

IcdMappingProfile IcdTableSettingsDialog::mapping() const
{
    return collectMappingFromUi();
}

QList<int> IcdTableSettingsDialog::mergedChildren() const
{
    QList<int> out;
    for (int i = 0; i < ui->lstJoin->count(); ++i)
    {
        QListWidgetItem* it = ui->lstJoin->item(i);
        if (it && it->checkState() == Qt::Checked)
            out << it->data(Qt::UserRole).toInt();
    }
    return out;
}

void IcdTableSettingsDialog::onAutoDetectClicked()
{
    if (m_tableIndex < 0 || m_tableIndex >= m_doc.tables.size())
        return;
    IcdMappingProfile sug = collectMappingFromUi();
    IcdDocxImporter::suggestMapping(m_doc.tables.at(m_tableIndex), sug);
    applyMappingToUi(sug);
}

void IcdTableSettingsDialog::onNameSourceChanged()
{
    const bool custom = (ui->cmbNameSource->currentIndex() == int(IcdNameSource::CustomPrefix));
    ui->txtName->setEnabled(custom);
}

void IcdTableSettingsDialog::onUnmergeAllClicked()
{
    for (int i = 0; i < ui->lstJoin->count(); ++i)
    {
        QListWidgetItem* it = ui->lstJoin->item(i);
        if (it)
            it->setCheckState(Qt::Unchecked);
    }
}

void IcdTableSettingsDialog::onSaveProfileClicked()
{
    IcdMappingProfile profile = collectMappingFromUi();
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

void IcdTableSettingsDialog::onLoadProfileClicked()
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
    applyMappingToUi(p);
}

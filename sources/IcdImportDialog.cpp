#include "IcdImportDialog.h"
#include "ui_IcdImportDialog.h"
#include "ui_IcdTablePreviewDialog.h"

#include "FieldCsvCodec.h"
#include "IcdDocxImporter.h"
#include "IcdReviewDraftBuilder.h"
#include "IcdTableSettingsDialog.h"
#include "InputValidator.h"
#include "Themes.h"

#include <QAbstractItemView>
#include <QByteArray>
#include <QDialog>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSet>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVariant>

namespace
{
QString elide(const QString& s, int n)
{
    const QString t = s.trimmed();
    if (t.size() <= n)
        return t;
    return t.left(n - 3) + "...";
}

const int TREE_COL_ITEM = 0;
const int TREE_COL_PORT = 1;
const int TREE_COL_LEN = 2;
const int TREE_COL_HEADER = 3;
const int TREE_COL_PREVIEW = 4;

const int SEL_COL_TABLE = 0;
const int SEL_COL_STATUS = 1;
const int SEL_COL_SETTINGS = 2;

const int FIELD_ROLE_INDEX = Qt::UserRole;
const int FIELD_ROLE_EXPR = Qt::UserRole + 1;

QString resolutionText(double v)
{
    return QString::number(v, 'g', 10);
}

bool collectFieldFromItem(QTreeWidgetItem* ci, const QString& messageName, int fieldRowNumber,
                          FieldDefinition& field, QStringList& errors)
{
    const QString fname = ci->text(TREE_COL_ITEM).trimmed();
    if (fname.isEmpty())
    {
        errors << QString("Message '%1' field row %2: field name is empty.")
                  .arg(messageName).arg(fieldRowNumber);
        return false;
    }

    bool offOk = false;
    const int byteOffset = ci->text(TREE_COL_PORT).trimmed().toInt(&offOk);
    if (!offOk || byteOffset < 1)
    {
        errors << QString("Message '%1' field '%2': invalid ByteOffset '%3'.")
                  .arg(messageName).arg(fname).arg(ci->text(TREE_COL_PORT));
        return false;
    }

    bool lenOk = false;
    const int length = ci->text(TREE_COL_LEN).trimmed().toInt(&lenOk);
    if (!lenOk || length < 1)
    {
        errors << QString("Message '%1' field '%2': invalid Length '%3'.")
                  .arg(messageName).arg(fname).arg(ci->text(TREE_COL_LEN));
        return false;
    }

    const QString typeText = ci->text(TREE_COL_HEADER).trimmed();
    FieldDataType dt = FieldDataType::RawUnsignedBE;
    if (!FieldCsvCodec::dataTypeFromLabel(typeText, dt))
    {
        errors << QString("Message '%1' field '%2': invalid DataType '%3'. Accepted: %4")
                  .arg(messageName).arg(fname).arg(typeText)
                  .arg(FieldCsvCodec::supportedDataTypeLabels().join(", "));
        return false;
    }

    double resolution = 1.0;
    const QString resText = ci->text(TREE_COL_PREVIEW).trimmed();
    if (!resText.isEmpty())
    {
        bool resOk = false;
        const double rv = resText.toDouble(&resOk);
        if (!resOk || rv <= 0.0)
        {
            errors << QString("Message '%1' field '%2': invalid Resolution '%3'.")
                      .arg(messageName).arg(fname).arg(resText);
            return false;
        }
        resolution = rv;
    }

    QString expr = ci->data(TREE_COL_ITEM, FIELD_ROLE_EXPR).toString().trimmed();
    if (expr.isEmpty())
        expr = "1";

    field = FieldDefinition();
    field.name = fname;
    field.byteOffset = byteOffset;
    field.byteOffsetcorrect = byteOffset - 1;
    field.length = length;
    field.dataType = dt;
    field.resolution = resolution;
    field.resolutionExpression = expr;
    field.nmeaFieldIndex = 0;
    return true;
}

const int MESSAGE_ROLE_DRAFT = Qt::UserRole;
}

IcdImportDialog::IcdImportDialog(QWidget* parent)
    : QDialog(parent),
      m_autoSeeded(false),
      ui(new Ui::IcdImportDialog)
{
    ui->setupUi(this);
    Themes::apply(this);

    // Review tree. Message rows use columns as: name / port / payload length /
    // optional header / Preview. Field rows use the same columns as: name /
    // byte offset / length / data type / resolution.
    ui->tree->setColumnCount(5);
    QStringList treeHeaders;
    treeHeaders << "Message / Field"
                << "Port / ByteOffset"
                << "Payload Len / Length"
                << "Optional Header / DataType"
                << "Preview / Resolution";
    ui->tree->setHeaderLabels(treeHeaders);
    ui->tree->setEditTriggers(QAbstractItemView::DoubleClicked
                              | QAbstractItemView::SelectedClicked
                              | QAbstractItemView::EditKeyPressed);

    // Selected-tables table (Table | Status | Settings button).
    ui->tblSelected->setColumnCount(3);
    QStringList selHeaders;
    selHeaders << "Table" << "Status" << "Settings";
    ui->tblSelected->setHorizontalHeaderLabels(selHeaders);
    ui->tblSelected->verticalHeader()->setVisible(false);
    ui->tblSelected->horizontalHeader()->setStretchLastSection(false);
    ui->tblSelected->horizontalHeader()->setSectionResizeMode(SEL_COL_TABLE, QHeaderView::Stretch);
    ui->tblSelected->horizontalHeader()->setSectionResizeMode(SEL_COL_STATUS, QHeaderView::ResizeToContents);
    ui->tblSelected->horizontalHeader()->setSectionResizeMode(SEL_COL_SETTINGS, QHeaderView::ResizeToContents);

    connect(ui->lstTables, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(onTableSelectionChanged()));
    connect(ui->btnBuild, SIGNAL(clicked()), this, SLOT(onBuildClicked()));
    connect(ui->btnAll, SIGNAL(clicked()), this, SLOT(onCheckAll()));
    connect(ui->btnNone, SIGNAL(clicked()), this, SLOT(onUncheckAll()));
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onAccept()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

IcdImportDialog::~IcdImportDialog()
{
    delete ui;
}

void IcdImportDialog::setDocument(const IcdDocument& doc)
{
    m_doc = doc;
    m_drafts.clear();
    m_result.clear();
    m_selectedTables.clear();
    m_parentOf.clear();
    m_tableMapping.clear();
    m_autoSeeded = false;

    ui->tree->clear();
    ui->txtWarnings->clear();

    populateTableList();
    onTableSelectionChanged();   // seed selection + continuation auto-grouping + box 2
}

QString IcdImportDialog::tableLabel(int tableIndex) const
{
    if (tableIndex < 0 || tableIndex >= m_doc.tables.size())
        return QString("Table %1").arg(tableIndex + 1);
    const IcdRawTable& t = m_doc.tables.at(tableIndex);
    QString heading = t.precedingHeading.trimmed();
    if (heading.isEmpty())
        heading = "(no heading)";
    return QString("Table %1: %2   [%3 x %4]")
        .arg(tableIndex + 1).arg(elide(heading, 50)).arg(t.rows.size()).arg(t.columnCount);
}

void IcdImportDialog::populateTableList()
{
    ui->lstTables->blockSignals(true);
    ui->lstTables->clear();
    for (int i = 0; i < m_doc.tables.size(); ++i)
    {
        const IcdRawTable& t = m_doc.tables.at(i);
        QString heading = t.precedingHeading.trimmed();
        if (heading.isEmpty())
            heading = "(no heading)";
        QListWidgetItem* item = new QListWidgetItem(
            QString("Table %1: %2   [%3 rows x %4 cols]")
                .arg(i + 1).arg(elide(heading, 60)).arg(t.rows.size()).arg(t.columnCount),
            ui->lstTables);
        item->setData(Qt::UserRole, i);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        const bool likelyFieldTable = (t.columnCount >= 3 && t.rows.size() >= 2);
        item->setCheckState(likelyFieldTable ? Qt::Checked : Qt::Unchecked);
    }
    ui->lstTables->blockSignals(false);
}

QList<int> IcdImportDialog::childrenOf(int parentIndex) const
{
    QList<int> out;
    for (int i = 0; i < m_selectedTables.size(); ++i)
    {
        const int c = m_selectedTables.at(i);
        if (c != parentIndex && m_parentOf.value(c, c) == parentIndex)
            out << c;
    }
    return out;
}

QList<int> IcdImportDialog::candidateChildrenFor(int parentIndex) const
{
    QList<int> out;
    for (int i = 0; i < m_selectedTables.size(); ++i)
    {
        const int c = m_selectedTables.at(i);
        if (c == parentIndex)
            continue;
        const int cp = m_parentOf.value(c, c);
        if (cp == parentIndex)                          // already a child of this parent
            out << c;
        else if (cp == c && childrenOf(c).isEmpty())    // a free standalone table
            out << c;
    }
    return out;
}

void IcdImportDialog::onTableSelectionChanged()
{
    // Recompute the ticked set (document order, as the list is built that way).
    QList<int> newSelected;
    for (int i = 0; i < ui->lstTables->count(); ++i)
    {
        QListWidgetItem* item = ui->lstTables->item(i);
        if (item && item->checkState() == Qt::Checked)
            newSelected << item->data(Qt::UserRole).toInt();
    }

    // Drop de-selected tables; free any children they parented.
    for (int i = 0; i < m_selectedTables.size(); ++i)
    {
        const int t = m_selectedTables.at(i);
        if (newSelected.contains(t))
            continue;
        const QList<int> kids = childrenOf(t);
        for (int k = 0; k < kids.size(); ++k)
            m_parentOf[kids.at(k)] = kids.at(k);
        m_parentOf.remove(t);
        m_tableMapping.remove(t);
    }

    // Newly selected tables start standalone with an auto-detected mapping.
    for (int i = 0; i < newSelected.size(); ++i)
    {
        const int t = newSelected.at(i);
        if (!m_parentOf.contains(t))
            m_parentOf.insert(t, t);
        if (!m_tableMapping.contains(t))
        {
            IcdMappingProfile p;
            if (t >= 0 && t < m_doc.tables.size())
                IcdDocxImporter::suggestMapping(m_doc.tables.at(t), p);
            m_tableMapping.insert(t, p);
        }
    }

    m_selectedTables = newSelected;

    // One-time structural pre-merge of likely continuation tables.
    if (!m_autoSeeded && !m_selectedTables.isEmpty())
    {
        QHash<int, int> parentOf;
        IcdDocxImporter::suggestContinuationGroups(m_doc, m_selectedTables, parentOf);
        for (QHash<int, int>::const_iterator it = parentOf.constBegin(); it != parentOf.constEnd(); ++it)
            m_parentOf[it.key()] = it.value();
        m_autoSeeded = true;
    }

    refreshSelectedTablesTable();
}

void IcdImportDialog::refreshSelectedTablesTable()
{
    QTableWidget* tbl = ui->tblSelected;
    tbl->setRowCount(0);
    for (int i = 0; i < m_selectedTables.size(); ++i)
    {
        const int t = m_selectedTables.at(i);
        const int row = tbl->rowCount();
        tbl->insertRow(row);

        QTableWidgetItem* it0 = new QTableWidgetItem(tableLabel(t));
        it0->setFlags(it0->flags() & ~Qt::ItemIsEditable);
        tbl->setItem(row, SEL_COL_TABLE, it0);

        const int parent = m_parentOf.value(t, t);
        QString status;
        if (parent == t)
        {
            const int nc = childrenOf(t).size();
            status = (nc > 0) ? QString("Parent (%1 merged)").arg(nc) : QString("Standalone");
        }
        else
        {
            status = QString("Merged into Table %1").arg(parent + 1);
        }
        QTableWidgetItem* it1 = new QTableWidgetItem(status);
        it1->setFlags(it1->flags() & ~Qt::ItemIsEditable);
        tbl->setItem(row, SEL_COL_STATUS, it1);

        QPushButton* btn = new QPushButton("Settings", tbl);
        btn->setProperty("tableIndex", t);
        btn->setEnabled(parent == t);   // a merged child is configured from its parent
        connect(btn, SIGNAL(clicked()), this, SLOT(onTableSettingsClicked()));
        tbl->setCellWidget(row, SEL_COL_SETTINGS, btn);
    }
}

void IcdImportDialog::onTableSettingsClicked()
{
    QObject* s = sender();
    if (!s)
        return;
    bool ok = false;
    const int t = s->property("tableIndex").toInt(&ok);
    if (ok)
        openSettingsForTable(t);
}

void IcdImportDialog::openSettingsForTable(int tableIndex)
{
    if (tableIndex < 0 || tableIndex >= m_doc.tables.size())
        return;
    if (m_parentOf.value(tableIndex, tableIndex) != tableIndex)
        return;   // child tables are configured from their parent

    const QList<int> candidates = candidateChildrenFor(tableIndex);
    QStringList labels;
    for (int i = 0; i < candidates.size(); ++i)
        labels << tableLabel(candidates.at(i));
    const QList<int> currentKids = childrenOf(tableIndex);

    IcdTableSettingsDialog dlg(this);
    dlg.setContext(m_doc, tableIndex, m_tableMapping.value(tableIndex),
                   candidates, labels, currentKids);
    if (dlg.exec() != QDialog::Accepted)
        return;

    m_tableMapping[tableIndex] = dlg.mapping();
    const QList<int> newKids = dlg.mergedChildren();
    for (int i = 0; i < candidates.size(); ++i)
    {
        const int c = candidates.at(i);
        if (newKids.contains(c))
            m_parentOf[c] = tableIndex;
        else if (m_parentOf.value(c, c) == tableIndex)
            m_parentOf[c] = c;   // was merged here, now unmerged
    }
    refreshSelectedTablesTable();
}

QList<IcdTableGroup> IcdImportDialog::buildGroups() const
{
    QList<IcdTableGroup> groups;
    for (int i = 0; i < m_selectedTables.size(); ++i)
    {
        const int t = m_selectedTables.at(i);
        if (m_parentOf.value(t, t) != t)
            continue;   // children are emitted with their parent
        IcdTableGroup g;
        g.mapping = m_tableMapping.value(t);
        g.tableIndices << t;
        const QList<int> kids = childrenOf(t);
        for (int k = 0; k < kids.size(); ++k)
            g.tableIndices << kids.at(k);
        groups << g;
    }
    return groups;
}

void IcdImportDialog::onBuildClicked()
{
    const QList<IcdTableGroup> groups = buildGroups();
    if (groups.isEmpty())
    {
        QMessageBox::warning(this, "Import ICD", "Tick at least one table to import.");
        return;
    }

    QStringList globalWarnings;
    IcdReviewDraftBuilder::buildGroupedDrafts(m_doc, groups, m_drafts, globalWarnings);
    populateReviewTree();

    QStringList all = globalWarnings;
    for (int i = 0; i < m_drafts.size(); ++i)
        all << m_drafts.at(i).warnings;
    ui->txtWarnings->setPlainText(all.isEmpty()
        ? QString("No warnings. Review the messages/fields below and click OK.")
        : all.join("\n"));
}

void IcdImportDialog::populateReviewTree()
{
    ui->tree->clear();
    for (int di = 0; di < m_drafts.size(); ++di)
    {
        const MessageDefinition& msg = m_drafts.at(di).message;
        QTreeWidgetItem* mi = new QTreeWidgetItem(ui->tree);
        mi->setText(TREE_COL_ITEM, msg.messageName);
        mi->setText(TREE_COL_PORT, QString::number(msg.port));
        mi->setText(TREE_COL_LEN, QString::number(msg.payloadLengthBytes));
        mi->setText(TREE_COL_HEADER, msg.optionalHeader.isEmpty()
            ? QString()
            : QString::fromLatin1(msg.optionalHeader.toHex()).toUpper());
        mi->setFlags(mi->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
        mi->setCheckState(TREE_COL_ITEM,
            (m_drafts.at(di).fieldRows.isEmpty() && msg.fields.isEmpty()) ? Qt::Unchecked : Qt::Checked);
        mi->setData(TREE_COL_ITEM, MESSAGE_ROLE_DRAFT, di);

        if (!m_drafts.at(di).fieldRows.isEmpty())
        {
            const QList<IcdFieldDraftRow>& rows = m_drafts.at(di).fieldRows;
            for (int fi = 0; fi < rows.size(); ++fi)
            {
                const IcdFieldDraftRow& r = rows.at(fi);
                QTreeWidgetItem* ci = new QTreeWidgetItem(mi);
                ci->setText(TREE_COL_ITEM, r.name);
                ci->setText(TREE_COL_PORT, r.byteOffsetText);
                ci->setText(TREE_COL_LEN, r.lengthText);
                ci->setText(TREE_COL_HEADER, r.dataTypeText);
                ci->setText(TREE_COL_PREVIEW, r.resolutionText);
                ci->setFlags(ci->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
                ci->setCheckState(TREE_COL_ITEM, Qt::Checked);
                ci->setData(TREE_COL_ITEM, FIELD_ROLE_INDEX, fi);
                ci->setData(TREE_COL_ITEM, FIELD_ROLE_EXPR, r.resolutionExpression);
            }
        }
        else
        {
            for (int fi = 0; fi < msg.fields.size(); ++fi)
            {
                const FieldDefinition& f = msg.fields.at(fi);
                QTreeWidgetItem* ci = new QTreeWidgetItem(mi);
                ci->setText(TREE_COL_ITEM, f.name);
                ci->setText(TREE_COL_PORT, QString::number(f.byteOffset));
                ci->setText(TREE_COL_LEN, QString::number(f.length));
                ci->setText(TREE_COL_HEADER, FieldCsvCodec::dataTypeToLabel(f.dataType));
                ci->setText(TREE_COL_PREVIEW, resolutionText(f.resolution));
                ci->setFlags(ci->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
                ci->setCheckState(TREE_COL_ITEM, Qt::Checked);
                ci->setData(TREE_COL_ITEM, FIELD_ROLE_INDEX, fi);
                ci->setData(TREE_COL_ITEM, FIELD_ROLE_EXPR, f.resolutionExpression);
            }
        }
        mi->setExpanded(true);

        QPushButton* pv = new QPushButton("Preview", ui->tree);
        pv->setProperty("tableIndex", m_drafts.at(di).sourceTableIndex);
        connect(pv, SIGNAL(clicked()), this, SLOT(onPreviewClicked()));
        ui->tree->setItemWidget(mi, TREE_COL_PREVIEW, pv);
    }
    ui->tree->resizeColumnToContents(TREE_COL_ITEM);
}

void IcdImportDialog::onPreviewClicked()
{
    QObject* s = sender();
    if (!s)
        return;
    bool ok = false;
    const int p = s->property("tableIndex").toInt(&ok);
    if (ok)
        previewGroup(p);
}

void IcdImportDialog::previewGroup(int parentIndex)
{
    if (parentIndex < 0 || parentIndex >= m_doc.tables.size())
        return;

    QList<int> members;
    members << parentIndex;
    members.append(childrenOf(parentIndex));

    int cols = 0;
    int totalRows = 0;
    for (int i = 0; i < members.size(); ++i)
    {
        const IcdRawTable& t = m_doc.tables.at(members.at(i));
        cols = qMax(cols, t.columnCount);
        totalRows += t.rows.size();
    }

    QDialog dlg(this);
    Ui::IcdTablePreviewDialog pv;
    pv.setupUi(&dlg);
    Themes::apply(&dlg);
    connect(pv.buttonBox, SIGNAL(accepted()), &dlg, SLOT(accept()));
    connect(pv.buttonBox, SIGNAL(rejected()), &dlg, SLOT(reject()));
    pv.lblPreviewTitle->setText(QString("%1   -   %2 table(s) merged, %3 rows")
                                    .arg(tableLabel(parentIndex)).arg(members.size()).arg(totalRows));

    pv.tblPreview->clear();
    pv.tblPreview->setColumnCount(cols);
    pv.tblPreview->setRowCount(totalRows);

    int r = 0;
    for (int mi = 0; mi < members.size(); ++mi)
    {
        const IcdRawTable& t = m_doc.tables.at(members.at(mi));
        for (int rowIdx = 0; rowIdx < t.rows.size(); ++rowIdx)
        {
            const QStringList& cells = t.rows.at(rowIdx);
            for (int c = 0; c < cols; ++c)
            {
                const QString cell = (c < cells.size()) ? cells.at(c) : QString();
                pv.tblPreview->setItem(r, c, new QTableWidgetItem(cell));
            }
            ++r;
        }
    }
    pv.tblPreview->resizeColumnsToContents();
    dlg.exec();
}

void IcdImportDialog::onCheckAll()
{
    for (int i = 0; i < ui->tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* mi = ui->tree->topLevelItem(i);
        mi->setCheckState(TREE_COL_ITEM, Qt::Checked);
        for (int j = 0; j < mi->childCount(); ++j)
            mi->child(j)->setCheckState(TREE_COL_ITEM, Qt::Checked);
    }
}

void IcdImportDialog::onUncheckAll()
{
    for (int i = 0; i < ui->tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* mi = ui->tree->topLevelItem(i);
        mi->setCheckState(TREE_COL_ITEM, Qt::Unchecked);
        for (int j = 0; j < mi->childCount(); ++j)
            mi->child(j)->setCheckState(TREE_COL_ITEM, Qt::Unchecked);
    }
}

void IcdImportDialog::onAccept()
{
    m_result.clear();
    QStringList errors;
    QSet<QString> usedNames;

    for (int i = 0; i < ui->tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* mi = ui->tree->topLevelItem(i);
        if (mi->checkState(TREE_COL_ITEM) != Qt::Checked)
            continue;

        const int di = mi->data(TREE_COL_ITEM, MESSAGE_ROLE_DRAFT).toInt();
        if (di < 0 || di >= m_drafts.size())
            continue;

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

            FieldDefinition f;
            if (collectFieldFromItem(ci, msg.messageName, j + 1, f, errors))
                selectedFields << f;
        }

        if (selectedFields.isEmpty())
        {
            errors << QString("Message '%1': no valid fields are ticked.").arg(msg.messageName);
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

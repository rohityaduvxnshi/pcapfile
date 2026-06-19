#include "IcdTablePickerDialog.h"
#include "ui_IcdTablePickerDialog.h"

#include "Themes.h"

#include <QHeaderView>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QUrl>

namespace
{
QString elide(const QString& s, int n)
{
    const QString t = s.trimmed();
    if (t.size() <= n)
        return t;
    return t.left(n - 3) + "...";
}
}

IcdTablePickerDialog::IcdTablePickerDialog(QWidget* parent)
    : QDialog(parent),
      m_populating(false),
      ui(new Ui::IcdTablePickerDialog)
{
    ui->setupUi(this);
    Themes::apply(this);

    ui->tblTables->setColumnCount(2);
    QStringList headers;
    headers << "Table" << "Size";
    ui->tblTables->setHorizontalHeaderLabels(headers);
    ui->tblTables->verticalHeader()->setVisible(false);
    ui->tblTables->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->tblTables->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    ui->splitter->setSizes(QList<int>() << 440 << 660);

    connect(ui->btnCheckAll, SIGNAL(clicked()), this, SLOT(onCheckAll()));
    connect(ui->btnUncheckAll, SIGNAL(clicked()), this, SLOT(onUncheckAll()));
    connect(ui->btnTogglePreview, SIGNAL(clicked()), this, SLOT(onTogglePreview()));
    connect(ui->tblTables, SIGNAL(cellClicked(int,int)), this, SLOT(onTableClicked(int,int)));
    connect(ui->tblTables, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(onItemChanged()));
    connect(ui->txtPreview, SIGNAL(anchorClicked(QUrl)), this, SLOT(onPreviewAnchorClicked(QUrl)));
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

IcdTablePickerDialog::~IcdTablePickerDialog()
{
    delete ui;
}

QString IcdTablePickerDialog::tableLabel(int tableIndex) const
{
    if (tableIndex < 0 || tableIndex >= m_doc.tables.size())
        return QString("Table %1").arg(tableIndex + 1);
    const IcdRawTable& t = m_doc.tables.at(tableIndex);
    QString heading = t.precedingHeading.trimmed();
    if (heading.isEmpty())
        heading = "(no heading)";
    return QString("Page %1, Table %2: %3")
        .arg(t.pageNumber).arg(t.tableOnPage).arg(elide(heading, 80));
}

void IcdTablePickerDialog::setDocument(const IcdDocument& doc)
{
    m_doc = doc;
    populateTableList();
    buildPreviewHtml();
    updateSummary();
}

void IcdTablePickerDialog::setPreselected(const QList<int>& tableIndices)
{
    m_populating = true;
    for (int i = 0; i < ui->tblTables->rowCount(); ++i)
    {
        QTableWidgetItem* item = ui->tblTables->item(i, 0);
        if (!item)
            continue;
        int idx = item->data(Qt::UserRole).toInt();
        item->setCheckState(tableIndices.contains(idx) ? Qt::Checked : Qt::Unchecked);
    }
    m_populating = false;
    updateSummary();
}

void IcdTablePickerDialog::populateTableList()
{
    m_populating = true;
    QTableWidget* tbl = ui->tblTables;
    tbl->setRowCount(0);
    for (int i = 0; i < m_doc.tables.size(); ++i)
    {
        const IcdRawTable& t = m_doc.tables.at(i);
        const int row = tbl->rowCount();
        tbl->insertRow(row);

        QTableWidgetItem* lbl = new QTableWidgetItem(tableLabel(i));
        lbl->setData(Qt::UserRole, i);
        lbl->setFlags((lbl->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        const bool likelyFieldTable = (t.columnCount >= 3 && t.rows.size() >= 2);
        lbl->setCheckState(likelyFieldTable ? Qt::Checked : Qt::Unchecked);
        QString heading = t.precedingHeading.trimmed();
        if (heading.isEmpty())
            heading = "(no heading)";
        lbl->setToolTip(QString("Full title: %1\nSize: %2 rows x %3 columns")
                            .arg(heading).arg(t.rows.size()).arg(t.columnCount));
        tbl->setItem(row, 0, lbl);

        QTableWidgetItem* sz = new QTableWidgetItem(
            QString("%1 x %2").arg(t.rows.size()).arg(t.columnCount));
        sz->setFlags(sz->flags() & ~Qt::ItemIsEditable);
        tbl->setItem(row, 1, sz);
    }
    m_populating = false;
}

void IcdTablePickerDialog::buildPreviewHtml()
{
    QString html;
    html += "<html><body style='font-family:sans-serif; font-size:10pt;'>";

    for (int i = 0; i < m_doc.tables.size(); ++i)
    {
        const IcdRawTable& t = m_doc.tables.at(i);
        QString heading = t.precedingHeading.trimmed();
        if (heading.isEmpty())
            heading = "(no heading)";

        html += QString("<a name=\"table_%1\"></a>").arg(i);
        html += QString("<h3><a href=\"#select_%1\" "
                        "style=\"text-decoration:none; color:inherit;\">"
                        "Page %2, Table %3: %4</a></h3>")
                    .arg(i).arg(t.pageNumber).arg(t.tableOnPage)
                    .arg(heading.toHtmlEscaped());

        html += "<table border='1' cellspacing='0' cellpadding='3' "
                "style='border-collapse:collapse; width:100%;'>";
        for (int r = 0; r < t.rows.size(); ++r)
        {
            html += "<tr>";
            const QStringList& cells = t.rows.at(r);
            const QString tag = (r == 0) ? "th" : "td";
            for (int c = 0; c < t.columnCount; ++c)
            {
                const QString cell = (c < cells.size()) ? cells.at(c).toHtmlEscaped() : QString();
                html += QString("<%1>%2</%1>").arg(tag).arg(cell);
            }
            html += "</tr>";
        }
        html += "</table><br/>";
    }
    html += "</body></html>";
    ui->txtPreview->setHtml(html);
}

void IcdTablePickerDialog::scrollPreviewToTable(int tableIndex)
{
    ui->txtPreview->scrollToAnchor(QString("table_%1").arg(tableIndex));
}

QList<int> IcdTablePickerDialog::selectedTables() const
{
    QList<int> result;
    for (int i = 0; i < ui->tblTables->rowCount(); ++i)
    {
        QTableWidgetItem* item = ui->tblTables->item(i, 0);
        if (item && item->checkState() == Qt::Checked)
            result << item->data(Qt::UserRole).toInt();
    }
    return result;
}

void IcdTablePickerDialog::onCheckAll()
{
    m_populating = true;
    for (int i = 0; i < ui->tblTables->rowCount(); ++i)
    {
        QTableWidgetItem* item = ui->tblTables->item(i, 0);
        if (item)
            item->setCheckState(Qt::Checked);
    }
    m_populating = false;
    updateSummary();
}

void IcdTablePickerDialog::onUncheckAll()
{
    m_populating = true;
    for (int i = 0; i < ui->tblTables->rowCount(); ++i)
    {
        QTableWidgetItem* item = ui->tblTables->item(i, 0);
        if (item)
            item->setCheckState(Qt::Unchecked);
    }
    m_populating = false;
    updateSummary();
}

void IcdTablePickerDialog::onTogglePreview()
{
    bool visible = ui->txtPreview->isVisible();
    ui->txtPreview->setVisible(!visible);
    ui->btnTogglePreview->setText(visible ? "Show Preview" : "Hide Preview");
}

void IcdTablePickerDialog::onTableClicked(int row, int /*column*/)
{
    QTableWidgetItem* item = ui->tblTables->item(row, 0);
    if (!item)
        return;
    int tableIndex = item->data(Qt::UserRole).toInt();
    scrollPreviewToTable(tableIndex);
}

void IcdTablePickerDialog::onPreviewAnchorClicked(const QUrl& url)
{
    const QString frag = url.fragment();
    if (!frag.startsWith("select_"))
        return;
    bool ok = false;
    int idx = frag.mid(7).toInt(&ok);
    if (!ok)
        return;
    for (int i = 0; i < ui->tblTables->rowCount(); ++i)
    {
        QTableWidgetItem* item = ui->tblTables->item(i, 0);
        if (item && item->data(Qt::UserRole).toInt() == idx)
        {
            ui->tblTables->selectRow(i);
            ui->tblTables->scrollTo(ui->tblTables->model()->index(i, 0));
            break;
        }
    }
}

void IcdTablePickerDialog::onItemChanged()
{
    if (!m_populating)
        updateSummary();
}

void IcdTablePickerDialog::updateSummary()
{
    int checked = 0;
    for (int i = 0; i < ui->tblTables->rowCount(); ++i)
    {
        QTableWidgetItem* item = ui->tblTables->item(i, 0);
        if (item && item->checkState() == Qt::Checked)
            ++checked;
    }
    ui->lblSummary->setText(QString("%1 of %2 tables selected")
                                .arg(checked).arg(m_doc.tables.size()));
}

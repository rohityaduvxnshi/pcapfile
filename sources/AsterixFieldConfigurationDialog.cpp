#include "AsterixFieldConfigurationDialog.h"
#include "ui_AsterixFieldConfigurationDialog.h"

#include "AsterixUapRegistry.h"
#include "BitfieldDecoder.h"
#include "BitfieldDecoderDialog.h"
#include "Themes.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVariant>
#include <QWidget>

namespace
{
const int COL_ENABLED       = 0;
const int COL_FRN           = 1;
const int COL_ITEM_ID       = 2;
const int COL_DEFAULT_NAME  = 3;
const int COL_CUSTOM_LABEL  = 4;
const int COL_KIND          = 5;
const int COL_VALUE_FORMAT  = 6;
const int COL_BIT_DECODER   = 7;
const int COLUMN_COUNT      = 8;

QString itemKindLabel(AsterixItemKind k)
{
    switch (k)
    {
    case AsterixItemKind::Fixed:          return "Fixed";
    case AsterixItemKind::Extended:       return "Extended";
    case AsterixItemKind::Repetitive:     return "Repetitive";
    case AsterixItemKind::Compound:       return "Compound";
    case AsterixItemKind::ExplicitLength: return "ExplicitLength";
    case AsterixItemKind::Unknown:        return "Unknown";
    }
    return "Unknown";
}

QString valueKindLabel(AsterixValueKind k)
{
    switch (k)
    {
    case AsterixValueKind::HexBytes:     return "Hex";
    case AsterixValueKind::UintBE:       return "Uint BE";
    case AsterixValueKind::IntBE:        return "Int BE";
    case AsterixValueKind::Float32BE:    return "Float32 BE";
    case AsterixValueKind::TimeOfDay:    return "Time of Day";
    case AsterixValueKind::Lat3byte:     return "Lat (3-byte)";
    case AsterixValueKind::Lon3byte:     return "Lon (3-byte)";
    case AsterixValueKind::Lat4byte:     return "Lat (4-byte)";
    case AsterixValueKind::Lon4byte:     return "Lon (4-byte)";
    case AsterixValueKind::Mode3A:       return "Mode 3/A";
    case AsterixValueKind::ModeC_FL:     return "Mode C / FL";
    case AsterixValueKind::Callsign6:    return "Callsign";
    case AsterixValueKind::Address24bit: return "24-bit Address";
    case AsterixValueKind::MultiPart:    return "Compound / Repetitive";
    }
    return "Hex";
}

// Bit decoder uses fixedLength for Fixed items; for other kinds it returns 0
// so we know to disable the button (variable-length items cannot be bit-decoded
// the same way the Hex pipeline does it, which assumes 1..8 byte fixed width).
int bitDecoderLengthFor(const AsterixItemDef& def)
{
    if (def.kind == AsterixItemKind::Fixed
        && def.fixedLength >= 1 && def.fixedLength <= 8)
    {
        return def.fixedLength;
    }
    return 0;
}
}

AsterixFieldConfigurationDialog::AsterixFieldConfigurationDialog(QWidget* parent)
    : QDialog(parent),
      m_category(0),
      ui(new Ui::AsterixFieldConfigurationDialog)
{
    ui->setupUi(this);
    Themes::apply(this);

    ui->tblItems->setColumnCount(COLUMN_COUNT);
    ui->tblItems->setHorizontalHeaderLabels(QStringList()
        << "Enabled" << "FRN" << "Item ID" << "Default Name"
        << "Custom Label" << "Kind" << "Value Format" << "Bit Decoder");
    ui->tblItems->horizontalHeader()->setStretchLastSection(false);
    ui->tblItems->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblItems->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

AsterixFieldConfigurationDialog::~AsterixFieldConfigurationDialog()
{
    delete ui;
}

void AsterixFieldConfigurationDialog::setCategory(int category)
{
    m_category = category;
    const AsterixCategoryDef* def = AsterixUapRegistry::lookup(category);
    m_rows.clear();
    if (def) m_rows.reserve(def->uap.size());

    if (def)
    {
        for (int i = 0; i < def->uap.size(); ++i)
        {
            RowState s;
            s.enabled = false;  // default off; user opts in per item
            s.customLabel.clear();
            s.hasBitDecoder = false;
            m_rows.append(s);
        }

        const QString display = QString("CAT%1 — %2 (%3 items)")
                                    .arg(category, 3, 10, QChar('0'))
                                    .arg(def->name)
                                    .arg(def->uap.size());
        ui->lblHeading->setText(display);
    }
    else
    {
        ui->lblHeading->setText("Unsupported ASTERIX category");
    }

    refreshTable();
}

void AsterixFieldConfigurationDialog::setExistingConfig(const QList<FieldDefinition>& fields)
{
    const AsterixCategoryDef* def = AsterixUapRegistry::lookup(m_category);
    if (!def) return;

    for (int i = 0; i < fields.size(); ++i)
    {
        const FieldDefinition& f = fields.at(i);
        if (f.asterixItemId.isEmpty()) continue;
        // Find the matching UAP row.
        for (int u = 0; u < def->uap.size() && u < m_rows.size(); ++u)
        {
            if (def->uap.at(u).id == f.asterixItemId)
            {
                m_rows[u].enabled = true;
                if (f.name != def->uap.at(u).defaultName)
                    m_rows[u].customLabel = f.name;
                m_rows[u].bitRules = f.bitDecodeRules;
                m_rows[u].hasBitDecoder = f.hasBitfieldDecoder
                                             && !f.bitDecodeRules.isEmpty();
                break;
            }
        }
    }
    refreshTable();
}

void AsterixFieldConfigurationDialog::refreshTable()
{
    const AsterixCategoryDef* def = AsterixUapRegistry::lookup(m_category);
    ui->tblItems->setRowCount(0);
    if (!def) return;

    for (int i = 0; i < def->uap.size(); ++i)
    {
        const AsterixItemDef& item = def->uap.at(i);
        const RowState& row = m_rows.at(i);
        const int r = ui->tblItems->rowCount();
        ui->tblItems->insertRow(r);

        // Enabled checkbox in a centred container so it cellWidget-aligns nicely.
        QWidget* checkHost = new QWidget(ui->tblItems);
        QCheckBox* check = new QCheckBox(checkHost);
        check->setChecked(row.enabled);
        check->setProperty("uapIndex", i);
        connect(check, SIGNAL(toggled(bool)), this, SLOT(onEnabledToggled(bool)));
        // Manual layout: a small QHBoxLayout in code would pull in QHBoxLayout
        // header; using move + resize keeps includes tight.
        check->setStyleSheet("margin-left:8px;");
        QHBoxLayout* h = new QHBoxLayout(checkHost);
        h->addWidget(check);
        h->setAlignment(Qt::AlignCenter);
        h->setContentsMargins(0, 0, 0, 0);
        checkHost->setLayout(h);
        ui->tblItems->setCellWidget(r, COL_ENABLED, checkHost);

        QTableWidgetItem* frnItem = new QTableWidgetItem(QString::number(item.frn));
        frnItem->setFlags(frnItem->flags() & ~Qt::ItemIsEditable);
        ui->tblItems->setItem(r, COL_FRN, frnItem);

        QTableWidgetItem* idItem = new QTableWidgetItem(item.id);
        idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
        ui->tblItems->setItem(r, COL_ITEM_ID, idItem);

        QTableWidgetItem* nameItem = new QTableWidgetItem(item.defaultName);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        ui->tblItems->setItem(r, COL_DEFAULT_NAME, nameItem);

        QTableWidgetItem* labelItem = new QTableWidgetItem(row.customLabel);
        // Custom label is the only editable cell.
        ui->tblItems->setItem(r, COL_CUSTOM_LABEL, labelItem);

        QTableWidgetItem* kindItem = new QTableWidgetItem(itemKindLabel(item.kind));
        kindItem->setFlags(kindItem->flags() & ~Qt::ItemIsEditable);
        ui->tblItems->setItem(r, COL_KIND, kindItem);

        QTableWidgetItem* valueItem = new QTableWidgetItem(valueKindLabel(item.valueKind));
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        ui->tblItems->setItem(r, COL_VALUE_FORMAT, valueItem);

        const int bitLen = bitDecoderLengthFor(item);
        QPushButton* bitBtn = new QPushButton(ui->tblItems);
        bitBtn->setProperty("uapIndex", i);
        if (bitLen <= 0)
        {
            bitBtn->setText("N/A");
            bitBtn->setEnabled(false);
            bitBtn->setToolTip("Bit decoding is available only for Fixed items with length 1..8 bytes.");
        }
        else if (row.hasBitDecoder && !row.bitRules.isEmpty())
        {
            bitBtn->setText(QString("Edit (%1 rules)").arg(row.bitRules.size()));
        }
        else
        {
            bitBtn->setText("Add");
        }
        connect(bitBtn, SIGNAL(clicked()), this, SLOT(onBitDecoderClicked()));
        ui->tblItems->setCellWidget(r, COL_BIT_DECODER, bitBtn);
    }

    ui->tblItems->resizeColumnsToContents();
    ui->tblItems->horizontalHeader()->setStretchLastSection(true);
}

int AsterixFieldConfigurationDialog::rowForBitButton(const QObject* obj) const
{
    if (!obj) return -1;
    bool ok = false;
    const int idx = obj->property("uapIndex").toInt(&ok);
    return ok ? idx : -1;
}

void AsterixFieldConfigurationDialog::onBitDecoderClicked()
{
    const AsterixCategoryDef* def = AsterixUapRegistry::lookup(m_category);
    if (!def) return;

    const int idx = rowForBitButton(sender());
    if (idx < 0 || idx >= def->uap.size() || idx >= m_rows.size()) return;

    const AsterixItemDef& item = def->uap.at(idx);
    const int bitLen = bitDecoderLengthFor(item);
    if (bitLen <= 0)
    {
        QMessageBox::information(this, "Bit Decoder",
            "This ASTERIX item is not a fixed 1..8 byte field — bit decoding is not supported for it.");
        return;
    }

    const QString fieldName = m_rows.at(idx).customLabel.isEmpty()
                                  ? item.defaultName : m_rows.at(idx).customLabel;

    BitfieldDecoderDialog dlg(fieldName, bitLen, m_rows.at(idx).bitRules, this);
    if (dlg.exec() == QDialog::Accepted)
    {
        m_rows[idx].bitRules = dlg.rules();
        m_rows[idx].hasBitDecoder = !m_rows[idx].bitRules.isEmpty();
        refreshTable();
    }
}

QList<FieldDefinition> AsterixFieldConfigurationDialog::fieldConfig() const
{
    QList<FieldDefinition> out;
    const AsterixCategoryDef* def = AsterixUapRegistry::lookup(m_category);
    if (!def) return out;

    for (int i = 0; i < def->uap.size() && i < m_rows.size(); ++i)
    {
        const RowState& row = m_rows.at(i);
        if (!row.enabled) continue;

        // Pull the user-typed label out of the table widget (so edits made after
        // the last refresh land here).
        QString label = row.customLabel;
        if (i < ui->tblItems->rowCount())
        {
            QTableWidgetItem* it = ui->tblItems->item(i, COL_CUSTOM_LABEL);
            if (it) label = it->text().trimmed();
        }
        if (label.isEmpty()) label = def->uap.at(i).defaultName;

        FieldDefinition f;
        f.name              = label;
        f.asterixItemId     = def->uap.at(i).id;
        f.bitDecodeRules    = row.bitRules;
        f.hasBitfieldDecoder = row.hasBitDecoder && !row.bitRules.isEmpty();
        // byteOffset / length / dataType remain at FieldDefinition defaults —
        // ignored by the ASTERIX export path.
        out.append(f);
    }
    return out;
}

void AsterixFieldConfigurationDialog::onEnabledToggled(bool checked)
{
    QObject* obj = sender();
    if (!obj) return;
    bool ok = false;
    const int idx = obj->property("uapIndex").toInt(&ok);
    if (!ok || idx < 0 || idx >= m_rows.size()) return;
    m_rows[idx].enabled = checked;
}

void AsterixFieldConfigurationDialog::onSaveClicked()
{
    // Sync edits in the Custom Label column back into m_rows.
    for (int i = 0; i < m_rows.size() && i < ui->tblItems->rowCount(); ++i)
    {
        QTableWidgetItem* it = ui->tblItems->item(i, COL_CUSTOM_LABEL);
        if (it) m_rows[i].customLabel = it->text().trimmed();
    }
    int enabledCount = 0;
    for (int i = 0; i < m_rows.size(); ++i)
        if (m_rows.at(i).enabled) ++enabledCount;

    if (enabledCount == 0)
    {
        QMessageBox::warning(this, "ASTERIX Fields",
            "Enable at least one UAP item, otherwise the message will produce empty CSV rows.");
        return;
    }
    accept();
}

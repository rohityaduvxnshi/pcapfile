#include "BitValueEditorDialog.h"
#include "ui_BitValueEditorDialog.h"

#include "PayloadBuilder.h"
#include "Themes.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>

namespace
{
const int GRP_COL_NAME = 0;
const int GRP_COL_BITS = 1;
const int GRP_COL_DEC  = 2;
const int GRP_COL_HEX  = 3;
}

BitValueEditorDialog::BitValueEditorDialog(const QString& fieldName,
                                           FieldDataType dataType,
                                           int length,
                                           double resolution,
                                           const QString& currentValueText,
                                           QWidget* parent)
    : QDialog(parent),
      m_syncing(false),
      m_refreshingGroups(false),
      ui(new Ui::BitValueEditorDialog)
{
    ui->setupUi(this);
    Themes::apply(this);

    ui->tblGroups->setColumnCount(4);
    ui->tblGroups->setHorizontalHeaderLabels(QStringList() << "Group name" << "Bits to include" << "Decimal" << "Hex");
    ui->tblGroups->horizontalHeader()->setStretchLastSection(true);
    ui->tblGroups->setSelectionBehavior(QAbstractItemView::SelectRows);
    connect(ui->btnAddGroup, SIGNAL(clicked()), this, SLOT(onAddGroupClicked()));
    connect(ui->btnRemoveGroup, SIGNAL(clicked()), this, SLOT(onRemoveGroupClicked()));
    connect(ui->tblGroups, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(onGroupCellChanged(QTableWidgetItem*)));

    m_field.name = fieldName;
    m_field.dataType = dataType;
    m_field.length = (length >= 1 && length <= 8) ? length : 1;
    m_field.resolution = (resolution > 0.0) ? resolution : 1.0;

    setWindowTitle(QString("Bit Editor — %1").arg(fieldName));
    ui->lblHeader->setText(QString("Field '%1' — %2 byte(s), resolution %3. "
                                   "Type a value OR toggle individual bits; both stay in sync in real time.")
                               .arg(fieldName)
                               .arg(m_field.length)
                               .arg(m_field.resolution));

    buildBitRows();

    connect(ui->txtTypedValue, SIGNAL(textEdited(QString)), this, SLOT(onTypedValueEdited(QString)));
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onOkClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    // Seed from the current Value cell. An empty or unparsable start simply
    // begins at raw 0 (all bits off).
    quint64 rawValue = 0;
    QString reason;
    QString solution;
    if (!currentValueText.trimmed().isEmpty()
        && PayloadBuilder::rawFromTypedValue(m_field, currentValueText, rawValue, reason, solution))
    {
        m_syncing = true;
        ui->txtTypedValue->setText(currentValueText.trimmed());
        m_syncing = false;
    }
    setChecksFromRaw(rawValue);
    updateReadouts(rawValue);
    refreshGroupValues(rawValue);
}

BitValueEditorDialog::~BitValueEditorDialog()
{
    delete ui;
}

QString BitValueEditorDialog::resultValueText() const
{
    return m_resultValueText;
}

// One row per byte, MSB byte first (byte 1 = first transmitted). Within a
// row the checkboxes run bit 7 .. bit 0 of that byte. m_bitChecks is indexed
// by the ABSOLUTE bit number where bit 0 is the LSB of the last byte.
void BitValueEditorDialog::buildBitRows()
{
    const int totalBits = m_field.length * 8;
    m_bitChecks.clear();
    for (int i = 0; i < totalBits; ++i)
        m_bitChecks.append(static_cast<QCheckBox*>(0));

    for (int byteIndex = 0; byteIndex < m_field.length; ++byteIndex)
    {
        QHBoxLayout* rowLayout = new QHBoxLayout();

        QString byteCaption = QString("Byte %1").arg(byteIndex + 1);
        if (m_field.length > 1 && byteIndex == 0)
            byteCaption += " (MSB)";
        else if (m_field.length > 1 && byteIndex == m_field.length - 1)
            byteCaption += " (LSB)";

        QLabel* lbl = new QLabel(byteCaption, this);
        lbl->setMinimumWidth(110);
        rowLayout->addWidget(lbl);

        for (int bitInByte = 7; bitInByte >= 0; --bitInByte)
        {
            const int absoluteBit = (m_field.length - 1 - byteIndex) * 8 + bitInByte;

            QCheckBox* check = new QCheckBox(QString::number(bitInByte), this);
            check->setToolTip(QString("Bit %1 of the whole field (bit 0 = least significant). "
                                      "Toggling it updates the typed value live.")
                                  .arg(absoluteBit));
            connect(check, SIGNAL(toggled(bool)), this, SLOT(onBitToggled(bool)));

            m_bitChecks[absoluteBit] = check;
            rowLayout->addWidget(check);
        }

        rowLayout->addStretch();
        ui->bitRowsLayout->addLayout(rowLayout);
    }
}

quint64 BitValueEditorDialog::rawFromChecks() const
{
    quint64 rawValue = 0;
    for (int bit = 0; bit < m_bitChecks.size(); ++bit)
    {
        if (m_bitChecks.at(bit) && m_bitChecks.at(bit)->isChecked())
            rawValue |= (1ULL << bit);
    }
    return rawValue;
}

void BitValueEditorDialog::setChecksFromRaw(quint64 rawValue)
{
    m_syncing = true;
    for (int bit = 0; bit < m_bitChecks.size(); ++bit)
    {
        QCheckBox* check = m_bitChecks.at(bit);
        if (!check)
            continue;
        check->blockSignals(true);
        check->setChecked((rawValue & (1ULL << bit)) != 0);
        check->blockSignals(false);
    }
    m_syncing = false;
}

void BitValueEditorDialog::updateReadouts(quint64 rawValue)
{
    ui->lblRawDec->setText(QString::number(static_cast<qulonglong>(rawValue)));

    QString hex = QString::number(static_cast<qulonglong>(rawValue), 16).toUpper();
    const int hexDigits = m_field.length * 2;
    while (hex.size() < hexDigits)
        hex.prepend('0');
    QString spaced;
    for (int i = 0; i < hex.size(); i += 2)
    {
        if (!spaced.isEmpty())
            spaced += ' ';
        spaced += hex.mid(i, 2);
    }
    ui->lblHex->setText(spaced);

    ui->lblFinalValue->setText(PayloadBuilder::typedValueFromRaw(m_field, rawValue));
}

void BitValueEditorDialog::onBitToggled(bool checked)
{
    Q_UNUSED(checked);
    if (m_syncing)
        return;

    const quint64 rawValue = rawFromChecks();

    m_syncing = true;
    ui->txtTypedValue->setText(PayloadBuilder::typedValueFromRaw(m_field, rawValue));
    ui->txtTypedValue->setStyleSheet(QString());
    m_lastReason.clear();
    m_lastSolution.clear();
    m_syncing = false;

    updateReadouts(rawValue);
    refreshGroupValues(rawValue);
}

void BitValueEditorDialog::onTypedValueEdited(const QString& text)
{
    if (m_syncing)
        return;

    quint64 rawValue = 0;
    QString reason;
    QString solution;
    if (!PayloadBuilder::rawFromTypedValue(m_field, text, rawValue, reason, solution))
    {
        // No popup per keystroke — mark the box red and remember why for OK.
        ui->txtTypedValue->setStyleSheet("border: 1px solid #e74c3c;");
        ui->lblFinalValue->setText(QString::fromUtf8("—"));
        m_lastReason = reason;
        m_lastSolution = solution;
        return;
    }

    ui->txtTypedValue->setStyleSheet(QString());
    m_lastReason.clear();
    m_lastSolution.clear();

    setChecksFromRaw(rawValue);
    updateReadouts(rawValue);
    refreshGroupValues(rawValue);
}

void BitValueEditorDialog::onOkClicked()
{
    const QString text = ui->txtTypedValue->text().trimmed();

    quint64 rawValue = 0;
    QString reason;
    QString solution;

    if (!text.isEmpty() && !PayloadBuilder::rawFromTypedValue(m_field, text, rawValue, reason, solution))
    {
        QMessageBox::warning(this, "Invalid Value",
                             QString("The typed value cannot be used: %1\nSolution: %2")
                                 .arg(reason.isEmpty() ? m_lastReason : reason)
                                 .arg(solution.isEmpty() ? m_lastSolution : solution));
        return;
    }

    if (text.isEmpty())
        rawValue = rawFromChecks();

    // Canonical text: what the parser would display for these bits.
    m_resultValueText = PayloadBuilder::typedValueFromRaw(m_field, rawValue);
    accept();
}

// ---------------------------------------------------------------- bit grouping
bool BitValueEditorDialog::parseBitSpec(const QString& text, QList<int>& bitsOut) const
{
    bitsOut.clear();
    const int totalBits = m_field.length * 8;
    const QStringList parts = text.split(',', QString::SkipEmptyParts);
    for (int i = 0; i < parts.size(); ++i)
    {
        const QString part = parts.at(i).trimmed();
        if (part.isEmpty())
            continue;
        if (part.contains('-'))
        {
            const QStringList ab = part.split('-');
            if (ab.size() != 2)
                return false;
            bool ok1 = false, ok2 = false;
            int a = ab.at(0).trimmed().toInt(&ok1);
            int b = ab.at(1).trimmed().toInt(&ok2);
            if (!ok1 || !ok2)
                return false;
            const int step = (a <= b) ? 1 : -1;
            for (int v = a; ; v += step)
            {
                if (v < 0 || v >= totalBits)
                    return false;
                bitsOut.append(v);
                if (v == b)
                    break;
            }
        }
        else
        {
            bool ok = false;
            const int v = part.toInt(&ok);
            if (!ok || v < 0 || v >= totalBits)
                return false;
            bitsOut.append(v);
        }
    }
    return true;
}

quint64 BitValueEditorDialog::groupValueFromRaw(const BitGroup& group, quint64 raw) const
{
    quint64 v = 0;
    for (int i = 0; i < group.bits.size() && i < 64; ++i)
    {
        const int b = group.bits.at(i);
        if (b >= 0 && b < m_field.length * 8 && ((raw >> b) & 1ULL))
            v |= (1ULL << i);
    }
    return v;
}

quint64 BitValueEditorDialog::applyGroupValue(const BitGroup& group, quint64 value, quint64 raw) const
{
    const int totalBits = m_field.length * 8;
    for (int i = 0; i < group.bits.size() && i < 64; ++i)
    {
        const int b = group.bits.at(i);
        if (b < 0 || b >= totalBits)
            continue;
        raw &= ~(1ULL << b);
        if ((value >> i) & 1ULL)
            raw |= (1ULL << b);
    }
    return raw;
}

void BitValueEditorDialog::refreshGroupValues(quint64 raw)
{
    m_refreshingGroups = true;
    for (int row = 0; row < ui->tblGroups->rowCount() && row < m_groups.size(); ++row)
    {
        const quint64 v = groupValueFromRaw(m_groups.at(row), raw);
        QTableWidgetItem* decItem = ui->tblGroups->item(row, GRP_COL_DEC);
        QTableWidgetItem* hexItem = ui->tblGroups->item(row, GRP_COL_HEX);
        if (decItem) decItem->setText(QString::number(static_cast<qulonglong>(v)));
        if (hexItem) hexItem->setText("0x" + QString::number(static_cast<qulonglong>(v), 16).toUpper());
    }
    m_refreshingGroups = false;
}

void BitValueEditorDialog::appendGroupRow(const BitGroup& group)
{
    m_refreshingGroups = true;
    const int row = ui->tblGroups->rowCount();
    ui->tblGroups->insertRow(row);
    ui->tblGroups->setItem(row, GRP_COL_NAME, new QTableWidgetItem(group.name));
    QStringList bitTexts;
    for (int i = 0; i < group.bits.size(); ++i)
        bitTexts << QString::number(group.bits.at(i));
    ui->tblGroups->setItem(row, GRP_COL_BITS, new QTableWidgetItem(bitTexts.join(",")));
    ui->tblGroups->setItem(row, GRP_COL_DEC, new QTableWidgetItem("0"));
    ui->tblGroups->setItem(row, GRP_COL_HEX, new QTableWidgetItem("0x0"));
    m_refreshingGroups = false;
}

void BitValueEditorDialog::onAddGroupClicked()
{
    BitGroup g;
    g.name = QString("Group %1").arg(m_groups.size() + 1);
    m_groups.append(g);
    appendGroupRow(g);
    refreshGroupValues(rawFromChecks());
}

void BitValueEditorDialog::onRemoveGroupClicked()
{
    const int row = ui->tblGroups->currentRow();
    if (row < 0 || row >= m_groups.size())
        return;
    m_refreshingGroups = true;
    ui->tblGroups->removeRow(row);
    m_groups.removeAt(row);
    m_refreshingGroups = false;
}

void BitValueEditorDialog::onGroupCellChanged(QTableWidgetItem* item)
{
    if (m_refreshingGroups || !item)
        return;
    const int row = item->row();
    const int col = item->column();
    if (row < 0 || row >= m_groups.size())
        return;

    if (col == GRP_COL_NAME)
    {
        m_groups[row].name = item->text();
        return;
    }

    if (col == GRP_COL_BITS)
    {
        QList<int> bits;
        if (!parseBitSpec(item->text(), bits))
        {
            QMessageBox::warning(this, "Bit grouping",
                QString("'%1' is not a valid bit list. Solution: use positions 0..%2, e.g. \"0-3\" or \"0,2,5\".")
                    .arg(item->text()).arg(m_field.length * 8 - 1));
            return;
        }
        m_groups[row].bits = bits;
        refreshGroupValues(rawFromChecks());
        return;
    }

    if (col == GRP_COL_DEC || col == GRP_COL_HEX)
    {
        const QString t = item->text().trimmed();
        bool ok = false;
        quint64 value = 0;
        if (col == GRP_COL_HEX || t.startsWith("0x", Qt::CaseInsensitive))
            value = (t.startsWith("0x", Qt::CaseInsensitive) ? t.mid(2) : t).toULongLong(&ok, 16);
        else
            value = t.toULongLong(&ok, 10);
        if (!ok)
            return; // ignore a half-typed value

        const quint64 newRaw = applyGroupValue(m_groups.at(row), value, rawFromChecks());
        setChecksFromRaw(newRaw);
        updateReadouts(newRaw);
        m_syncing = true;
        ui->txtTypedValue->setText(PayloadBuilder::typedValueFromRaw(m_field, newRaw));
        ui->txtTypedValue->setStyleSheet(QString());
        m_syncing = false;
        refreshGroupValues(newRaw);
    }
}

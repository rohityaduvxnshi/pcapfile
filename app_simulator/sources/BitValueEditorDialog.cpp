#include "BitValueEditorDialog.h"
#include "ui_BitValueEditorDialog.h"

#include "PayloadBuilder.h"
#include "Themes.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>

BitValueEditorDialog::BitValueEditorDialog(const QString& fieldName,
                                           FieldDataType dataType,
                                           int length,
                                           double resolution,
                                           const QString& currentValueText,
                                           QWidget* parent)
    : QDialog(parent),
      m_syncing(false),
      ui(new Ui::BitValueEditorDialog)
{
    ui->setupUi(this);
    Themes::apply(this);

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

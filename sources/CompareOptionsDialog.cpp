#include "CompareOptionsDialog.h"
#include "ui_CompareOptionsDialog.h"

#include "Themes.h"

#include <QByteArray>
#include <QChar>
#include <QMessageBox>
#include <QStringList>

namespace
{

bool isHexString(const QString& s)
{
    for (int i = 0; i < s.size(); ++i)
    {
        const QChar c = s.at(i);
        if (!(c.isDigit() || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}

bool decodeExpectedBytes(const QString& text, const QString& mode, int declaredLength,
                        QByteArray& out, QString& errorOut)
{
    out.clear();
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return true;  // blank = log-only mode, not an error

    if (mode == "HEX")
    {
        QString hex = trimmed;
        hex.remove(' ');
        if ((hex.size() % 2) != 0)
        {
            errorOut = "Hex value must have an even number of characters.";
            return false;
        }
        if (!isHexString(hex))
        {
            errorOut = "Hex value contains non-hex characters.";
            return false;
        }
        out = QByteArray::fromHex(hex.toLatin1());
    }
    else  // ASCII
    {
        out = trimmed.toUtf8();
    }

    if (declaredLength > 0 && out.size() != declaredLength)
    {
        errorOut = QString("Expected value is %1 byte(s) but declared length is %2.")
                       .arg(out.size()).arg(declaredLength);
        return false;
    }
    return true;
}

} // namespace

CompareOptionsDialog::CompareOptionsDialog(QWidget* parent)
    : QDialog(parent),
      ui(new Ui::CompareOptionsDialog),
      m_payloadLengthBytes(0)
{
    ui->setupUi(this);
    Themes::apply(this);

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onSaveClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

CompareOptionsDialog::~CompareOptionsDialog()
{
    delete ui;
}

void CompareOptionsDialog::setPayloadLength(int payloadLengthBytes)
{
    m_payloadLengthBytes = payloadLengthBytes;
    if (payloadLengthBytes > 0)
    {
        ui->spinHdrOffset->setMaximum(payloadLengthBytes);
        ui->spinHdrLength->setMaximum(payloadLengthBytes);
        ui->spinTermOffset->setMaximum(payloadLengthBytes);
        ui->spinTermLength->setMaximum(payloadLengthBytes);
        ui->spinCsumStart->setMaximum(payloadLengthBytes);
        ui->spinCsumEnd->setMaximum(payloadLengthBytes);
        ui->spinCsumOffset->setMaximum(payloadLengthBytes);
    }
}

void CompareOptionsDialog::setHasCompareOptions(bool /*enabled*/)
{
    // Reserved for future toggle; current dialog always edits a config — the master
    // hasCompareOptions flag is derived from "any section enabled" on Save.
}

void CompareOptionsDialog::setConfig(const CompareOptionsConfig& c)
{
    // Header
    ui->grpHeader->setChecked(c.checkHeader);
    ui->spinHdrOffset->setValue(c.headerByteOffset);
    ui->spinHdrLength->setValue(c.headerLength);
    ui->cmbHdrMode->setCurrentText(c.headerInputMode);
    ui->txtHdrExpected->setText(c.expectedHeaderText);

    // Terminator
    ui->grpTerminator->setChecked(c.checkTerminator);
    ui->spinTermOffset->setValue(c.terminatorByteOffset);
    ui->spinTermLength->setValue(c.terminatorLength);
    ui->cmbTermMode->setCurrentText(c.terminatorInputMode);
    ui->txtTermExpected->setText(c.expectedTerminatorText);

    // Checksum
    ui->grpChecksum->setChecked(c.checkChecksum);
    ui->cmbCsumAlgo->setCurrentText(c.checksumAlgorithm);
    ui->spinCsumStart->setValue(c.checksumRangeStart);
    ui->spinCsumEnd->setValue(c.checksumRangeEnd);
    ui->spinCsumOffset->setValue(c.checksumByteOffset);
    ui->spinCsumLength->setValue(c.checksumLength > 0 ? c.checksumLength : 1);

    // Refresh rate
    ui->grpRefreshRate->setChecked(c.checkRefreshRate);
    ui->dspRRExpected->setValue(c.expectedRefreshRateHz);
    ui->dspRRTolerance->setValue(c.refreshRateToleranceHz);

    // Endianness
    ui->grpEndian->setChecked(c.checkEndianness);
    if (!c.expectedEndianness.isEmpty())
        ui->cmbEndian->setCurrentText(c.expectedEndianness);
    else
        ui->cmbEndian->setCurrentText("(don't compare)");
}

CompareOptionsConfig CompareOptionsDialog::config() const
{
    CompareOptionsConfig c;

    c.checkHeader = ui->grpHeader->isChecked();
    c.headerByteOffset = ui->spinHdrOffset->value();
    c.headerLength = ui->spinHdrLength->value();
    c.headerInputMode = ui->cmbHdrMode->currentText();
    c.expectedHeaderText = ui->txtHdrExpected->text().trimmed();
    QByteArray dummy;
    QString err;
    decodeExpectedBytes(c.expectedHeaderText, c.headerInputMode, c.headerLength,
                       c.expectedHeader, err);

    c.checkTerminator = ui->grpTerminator->isChecked();
    c.terminatorByteOffset = ui->spinTermOffset->value();
    c.terminatorLength = ui->spinTermLength->value();
    c.terminatorInputMode = ui->cmbTermMode->currentText();
    c.expectedTerminatorText = ui->txtTermExpected->text().trimmed();
    decodeExpectedBytes(c.expectedTerminatorText, c.terminatorInputMode, c.terminatorLength,
                       c.expectedTerminator, err);

    c.checkChecksum = ui->grpChecksum->isChecked();
    c.checksumAlgorithm = ui->cmbCsumAlgo->currentText();
    c.checksumRangeStart = ui->spinCsumStart->value();
    c.checksumRangeEnd = ui->spinCsumEnd->value();
    c.checksumByteOffset = ui->spinCsumOffset->value();
    c.checksumLength = ui->spinCsumLength->value();

    c.checkRefreshRate = ui->grpRefreshRate->isChecked();
    c.expectedRefreshRateHz = ui->dspRRExpected->value();
    c.refreshRateToleranceHz = ui->dspRRTolerance->value();

    c.checkEndianness = ui->grpEndian->isChecked();
    const QString endChoice = ui->cmbEndian->currentText();
    c.expectedEndianness = (endChoice == "(don't compare)") ? QString() : endChoice;

    return c;
}

bool CompareOptionsDialog::hasCompareOptions() const
{
    return ui->grpHeader->isChecked()
        || ui->grpTerminator->isChecked()
        || ui->grpChecksum->isChecked()
        || ui->grpRefreshRate->isChecked()
        || ui->grpEndian->isChecked();
}

void CompareOptionsDialog::onSaveClicked()
{
    QStringList errors;

    if (ui->grpHeader->isChecked())
    {
        const int off = ui->spinHdrOffset->value();
        const int len = ui->spinHdrLength->value();
        if (len <= 0)
            errors << "Header: length must be > 0.";
        if (m_payloadLengthBytes > 0 && off + len > m_payloadLengthBytes)
            errors << QString("Header: offset+length (%1) exceeds payload length (%2).")
                         .arg(off + len).arg(m_payloadLengthBytes);

        QByteArray bytes;
        QString err;
        if (!decodeExpectedBytes(ui->txtHdrExpected->text(), ui->cmbHdrMode->currentText(),
                                 len, bytes, err))
            errors << QString("Header: %1").arg(err);
    }

    if (ui->grpTerminator->isChecked())
    {
        const int off = ui->spinTermOffset->value();
        const int len = ui->spinTermLength->value();
        if (len <= 0)
            errors << "Terminator: length must be > 0.";
        if (off >= 0 && m_payloadLengthBytes > 0 && off + len > m_payloadLengthBytes)
            errors << QString("Terminator: offset+length (%1) exceeds payload length (%2).")
                         .arg(off + len).arg(m_payloadLengthBytes);

        QByteArray bytes;
        QString err;
        if (!decodeExpectedBytes(ui->txtTermExpected->text(), ui->cmbTermMode->currentText(),
                                 len, bytes, err))
            errors << QString("Terminator: %1").arg(err);
    }

    if (ui->grpChecksum->isChecked())
    {
        const int start = ui->spinCsumStart->value();
        const int end = ui->spinCsumEnd->value();
        const int off = ui->spinCsumOffset->value();
        const int len = ui->spinCsumLength->value();
        if (start >= end)
            errors << "Checksum: range start must be less than range end.";
        if (len <= 0 || len > 4)
            errors << "Checksum: stored length must be 1-4 bytes.";
        if (m_payloadLengthBytes > 0)
        {
            if (end > m_payloadLengthBytes)
                errors << QString("Checksum: range end (%1) exceeds payload length (%2).")
                             .arg(end).arg(m_payloadLengthBytes);
            if (off + len > m_payloadLengthBytes)
                errors << QString("Checksum: stored offset+length (%1) exceeds payload length (%2).")
                             .arg(off + len).arg(m_payloadLengthBytes);
        }
    }

    if (ui->grpRefreshRate->isChecked())
    {
        if (ui->dspRRExpected->value() < 0.0)
            errors << "Refresh rate: expected Hz must be >= 0.";
        if (ui->dspRRTolerance->value() < 0.0)
            errors << "Refresh rate: tolerance Hz must be >= 0.";
    }

    if (!errors.isEmpty())
    {
        QMessageBox::warning(this, "Invalid Compare Options", errors.join("\n"));
        return;
    }

    accept();
}

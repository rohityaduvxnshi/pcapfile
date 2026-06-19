#include "SerialDataSender.h"

SerialDataSender::SerialDataSender(QObject* parent)
    : DataSender(parent)
    , m_port(0)
    , m_baudRate(115200)
    , m_dataBits(8)
    , m_parity("None")
    , m_stopBits("1")
{
}

SerialDataSender::~SerialDataSender()
{
    close();
}

void SerialDataSender::configure(const QString& portName,
                                 int baudRate,
                                 int dataBits,
                                 const QString& parity,
                                 const QString& stopBits)
{
    m_portName = portName.trimmed();
    m_baudRate = baudRate;
    m_dataBits = dataBits;
    m_parity = parity.trimmed();
    m_stopBits = stopBits.trimmed();
}

bool SerialDataSender::open(QString& errorMessage)
{
    close();

    if (m_portName.isEmpty())
    {
        errorMessage = "No serial port has been selected. "
                       "Solution: press Refresh and pick one of the listed COM ports.";
        return false;
    }

    if (m_baudRate <= 0)
    {
        errorMessage = QString("'%1' is not a valid baud rate. "
                               "Solution: pick a preset (e.g. 115200) or type the rate your device expects.")
                           .arg(m_baudRate);
        return false;
    }

    m_port = new QSerialPort(this);
    m_port->setPortName(m_portName);
    m_port->setBaudRate(m_baudRate);

    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    if (m_dataBits == 5) dataBits = QSerialPort::Data5;
    else if (m_dataBits == 6) dataBits = QSerialPort::Data6;
    else if (m_dataBits == 7) dataBits = QSerialPort::Data7;
    m_port->setDataBits(dataBits);

    QSerialPort::Parity parity = QSerialPort::NoParity;
    const QString parityLower = m_parity.toLower();
    if (parityLower == "even") parity = QSerialPort::EvenParity;
    else if (parityLower == "odd") parity = QSerialPort::OddParity;
    else if (parityLower == "mark") parity = QSerialPort::MarkParity;
    else if (parityLower == "space") parity = QSerialPort::SpaceParity;
    m_port->setParity(parity);

    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    if (m_stopBits == "2") stopBits = QSerialPort::TwoStop;
    else if (m_stopBits == "1.5") stopBits = QSerialPort::OneAndHalfStop;
    m_port->setStopBits(stopBits);

    m_port->setFlowControl(QSerialPort::NoFlowControl);

    connect(m_port, SIGNAL(errorOccurred(QSerialPort::SerialPortError)),
            this, SLOT(onPortErrorOccurred(QSerialPort::SerialPortError)));

    if (!m_port->open(QIODevice::WriteOnly))
    {
        const QSerialPort::SerialPortError openError = m_port->error();
        QString hint;
        if (openError == QSerialPort::PermissionError)
            hint = QString("The port is probably open in another program. "
                           "Solution: close the other program using %1 (a terminal, Qt Creator, "
                           "another simulator instance), or pick a different port.").arg(m_portName);
        else if (openError == QSerialPort::DeviceNotFoundError)
            hint = QString("%1 does not exist (any more). "
                           "Solution: press Refresh and pick one of the listed ports — "
                           "the adapter may have been unplugged or renumbered.").arg(m_portName);
        else
            hint = "Solution: check the cable and the port settings, press Refresh and try again.";

        errorMessage = QString("Could not open %1: %2. %3")
                           .arg(m_portName)
                           .arg(m_port->errorString())
                           .arg(hint);
        close();
        return false;
    }

    return true;
}

bool SerialDataSender::send(const QByteArray& payload, QString& errorMessage)
{
    if (!m_port || !m_port->isOpen())
    {
        errorMessage = "The serial link is not connected. Solution: press Connect first.";
        return false;
    }

    const qint64 written = m_port->write(payload);
    if (written != payload.size())
    {
        errorMessage = QString("Writing to %1 failed: %2. "
                               "Solution: check the cable, then Disconnect and Connect again.")
                           .arg(m_portName)
                           .arg(m_port->errorString());
        return false;
    }

    m_port->flush();
    return true;
}

void SerialDataSender::close()
{
    if (m_port)
    {
        if (m_port->isOpen())
            m_port->close();
        m_port->deleteLater();
        m_port = 0;
    }
}

bool SerialDataSender::isOpen() const
{
    return m_port && m_port->isOpen();
}

QString SerialDataSender::description() const
{
    const QString parityInitial = m_parity.isEmpty() ? QString("N") : m_parity.left(1).toUpper();
    return QString("Serial %1 @ %2, %3-%4-%5")
        .arg(m_portName)
        .arg(m_baudRate)
        .arg(m_dataBits)
        .arg(parityInitial)
        .arg(m_stopBits);
}

bool SerialDataSender::waitForSent(int msecs, QString& errorMessage)
{
    if (!m_port || !m_port->isOpen())
    {
        errorMessage = "The serial link is not connected. Solution: press Connect first.";
        return false;
    }

    if (m_port->bytesToWrite() <= 0)
        return true;

    if (!m_port->waitForBytesWritten(msecs))
    {
        errorMessage = QString("The health message could not be transmitted on %1 within %2 ms (%3). "
                               "Solution: check the cable and the baud rate, then try Connect again.")
                           .arg(m_portName)
                           .arg(msecs)
                           .arg(m_port->errorString());
        return false;
    }

    return true;
}

void SerialDataSender::onPortErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;

    const QString text = m_port ? m_port->errorString() : QString("unknown error");
    emit linkError(QString("Serial port %1 reported an error: %2. "
                           "Solution: check the cable/adapter, then Disconnect and Connect again.")
                       .arg(m_portName)
                       .arg(text));
}

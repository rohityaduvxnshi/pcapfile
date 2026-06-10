#include "SerialPortReceiver.h"

#include <QSerialPort>

SerialPortReceiver::SerialPortReceiver(QObject *parent)
    : QObject(parent)
{
}

SerialPortReceiver::~SerialPortReceiver()
{
    stop();
}

void SerialPortReceiver::configure(const QString &portName, int baudRate,
                                   int dataBits, const QString &parity, int stopBits)
{
    m_portName = portName.trimmed();
    m_baudRate = baudRate;
    m_dataBits = dataBits;
    m_parity = parity;
    m_stopBits = stopBits;
}

bool SerialPortReceiver::start(QString &errorOut)
{
    errorOut.clear();

    if (m_running || m_port)
        stop();

    if (m_portName.isEmpty())
    {
        errorOut = "No serial port selected.";
        return false;
    }

    m_port = new QSerialPort(this);
    m_port->setPortName(m_portName);
    m_port->setBaudRate(m_baudRate);

    switch (m_dataBits)
    {
    case 5: m_port->setDataBits(QSerialPort::Data5); break;
    case 6: m_port->setDataBits(QSerialPort::Data6); break;
    case 7: m_port->setDataBits(QSerialPort::Data7); break;
    default: m_port->setDataBits(QSerialPort::Data8); break;
    }

    if (m_parity.compare(QLatin1String("Even"), Qt::CaseInsensitive) == 0)
        m_port->setParity(QSerialPort::EvenParity);
    else if (m_parity.compare(QLatin1String("Odd"), Qt::CaseInsensitive) == 0)
        m_port->setParity(QSerialPort::OddParity);
    else
        m_port->setParity(QSerialPort::NoParity);

    m_port->setStopBits(m_stopBits == 2 ? QSerialPort::TwoStop : QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port->open(QIODevice::ReadOnly))
    {
        errorOut = QString("Could not open %1: %2")
                       .arg(m_portName).arg(m_port->errorString());
        delete m_port;
        m_port = nullptr;
        return false;
    }

    connect(m_port, SIGNAL(readyRead()), this, SLOT(onReadyRead()));

    m_buffer.clear();
    m_bytesReceived = 0;
    m_running = true;
    return true;
}

void SerialPortReceiver::stop()
{
    if (m_port)
    {
        m_port->disconnect(this);
        if (m_port->isOpen())
            m_port->close();
        m_port->deleteLater();
        m_port = nullptr;
    }
    m_buffer.clear();
    m_running = false;
}

bool SerialPortReceiver::isRunning() const
{
    return m_running;
}

quint64 SerialPortReceiver::bytesReceived() const
{
    return m_bytesReceived;
}

void SerialPortReceiver::onReadyRead()
{
    if (!m_port)
        return;

    const QByteArray chunk = m_port->readAll();
    if (chunk.isEmpty())
        return;

    m_bytesReceived += static_cast<quint64>(chunk.size());
    m_buffer.append(chunk);

    // Guard against a line-less binary stream growing the buffer forever.
    const int MAX_BUFFER = 1 << 20;   // 1 MB
    if (m_buffer.size() > MAX_BUFFER)
    {
        m_buffer.clear();
        emit portError(QStringLiteral(
            "Serial buffer overflowed without a line terminator; data discarded. "
            "Serial Mode expects newline-terminated records (NMEA sentences or hex text lines)."));
        return;
    }

    // Emit every complete line currently in the buffer.
    while (true)
    {
        const int nl = m_buffer.indexOf('\n');
        if (nl < 0)
            break;
        QByteArray line = m_buffer.left(nl);
        m_buffer.remove(0, nl + 1);
        while (line.endsWith('\r'))
            line.chop(1);
        if (!line.trimmed().isEmpty())
            emit lineReceived(line, QDateTime::currentDateTimeUtc());
    }
}

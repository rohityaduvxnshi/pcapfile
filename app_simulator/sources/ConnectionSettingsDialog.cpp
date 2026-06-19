#include "ConnectionSettingsDialog.h"
#include "ui_ConnectionSettingsDialog.h"

#include "DataSender.h"
#include "SerialDataSender.h"
#include "TcpDataSender.h"
#include "Themes.h"
#include "UdpDataSender.h"

#include <QMessageBox>
#include <QSerialPortInfo>
#include <QShowEvent>

ConnectionSettingsDialog::ConnectionSettingsDialog(QWidget* parent)
    : QDialog(parent),
      m_sender(0),
      m_dotState("gray"),
      ui(new Ui::ConnectionSettingsDialog)
{
    ui->setupUi(this);
    Themes::apply(this);

    connect(ui->cmbDestinationType, SIGNAL(currentIndexChanged(int)), this, SLOT(onDestinationTypeChanged(int)));
    connect(ui->btnRefreshSerialPorts, SIGNAL(clicked()), this, SLOT(onRefreshSerialPortsClicked()));
    connect(ui->btnConnect, SIGNAL(clicked()), this, SLOT(onConnectClicked()));
    connect(ui->btnDisconnect, SIGNAL(clicked()), this, SLOT(onDisconnectClicked()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(hide()));

    ui->stackDestination->setCurrentIndex(0);
    setLinkDot("gray");
    onRefreshSerialPortsClicked();
    updateUiState();
}

ConnectionSettingsDialog::~ConnectionSettingsDialog()
{
    dropSender();
    delete ui;
}

bool ConnectionSettingsDialog::isConnected() const
{
    return m_sender && m_sender->isOpen();
}

DataSender* ConnectionSettingsDialog::activeSender() const
{
    return isConnected() ? m_sender : 0;
}

QString ConnectionSettingsDialog::connectionName() const
{
    return isConnected() ? m_sender->description() : QString("Not connected");
}

QString ConnectionSettingsDialog::dotState() const
{
    return m_dotState;
}

void ConnectionSettingsDialog::refreshSerialPorts()
{
    onRefreshSerialPortsClicked();
}

void ConnectionSettingsDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    setLinkDot(m_dotState);   // the dot's stylesheet must survive a theme toggle while hidden
}

void ConnectionSettingsDialog::onDestinationTypeChanged(int index)
{
    // Combo order matches the stack pages: 0 = UDP, 1 = TCP, 2 = Serial.
    ui->stackDestination->setCurrentIndex(index);
}

void ConnectionSettingsDialog::onRefreshSerialPortsClicked()
{
    const QString previous = ui->cmbSerialPort->currentText();
    ui->cmbSerialPort->clear();

    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (int i = 0; i < ports.size(); ++i)
    {
        ui->cmbSerialPort->addItem(ports.at(i).portName());
        ui->cmbSerialPort->setItemData(i,
            QString("%1 - %2").arg(ports.at(i).portName()).arg(ports.at(i).description()),
            Qt::ToolTipRole);
    }

    if (!previous.trimmed().isEmpty())
    {
        const int existingIndex = ui->cmbSerialPort->findText(previous);
        if (existingIndex >= 0)
            ui->cmbSerialPort->setCurrentIndex(existingIndex);
        else
            ui->cmbSerialPort->setEditText(previous);
    }
}

DataSender* ConnectionSettingsDialog::buildSenderFromUi()
{
    switch (ui->cmbDestinationType->currentIndex())
    {
    case 1: // Ethernet (TCP)
    {
        TcpDataSender* tcp = new TcpDataSender(this);
        tcp->setDestination(ui->txtTcpIp->text(), ui->spinTcpPort->value());
        return tcp;
    }
    case 2: // Serial Port
    {
        SerialDataSender* serial = new SerialDataSender(this);
        serial->configure(ui->cmbSerialPort->currentText(),
                          ui->cmbSerialBaud->currentText().trimmed().toInt(),
                          ui->cmbSerialDataBits->currentText().trimmed().toInt(),
                          ui->cmbSerialParity->currentText(),
                          ui->cmbSerialStopBits->currentText());
        return serial;
    }
    default: // 0 = Ethernet (UDP)
    {
        UdpDataSender* udp = new UdpDataSender(this);
        udp->setDestination(ui->txtUdpIp->text(), ui->spinUdpPort->value());
        return udp;
    }
    }
}

void ConnectionSettingsDialog::dropSender()
{
    if (m_sender)
    {
        m_sender->close();
        m_sender->deleteLater();
        m_sender = 0;
    }
}

void ConnectionSettingsDialog::setLinkDot(const QString& state)
{
    m_dotState = state;
    QString color = "#94A3B8"; // gray
    if (state == "green")
        color = "#22C55E";
    else if (state == "red")
        color = "#EF4444";
    ui->lblLinkDot->setStyleSheet(QString("color:%1; font-size:16pt; background:transparent; border:none;").arg(color));
}

void ConnectionSettingsDialog::updateUiState()
{
    const bool connected = isConnected();
    ui->btnConnect->setEnabled(!connected);
    ui->btnDisconnect->setEnabled(connected);
    ui->cmbDestinationType->setEnabled(!connected);
    ui->stackDestination->setEnabled(!connected);
}

void ConnectionSettingsDialog::onConnectClicked()
{
    dropSender();

    m_sender = buildSenderFromUi();
    connect(m_sender, SIGNAL(linkError(QString)), this, SLOT(onSenderLinkError(QString)));

    QString error;
    if (!m_sender->open(error))
    {
        dropSender();
        setLinkDot("red");
        ui->lblLinkStatus->setText("Connection failed");
        updateUiState();
        emit connectionChanged();
        QMessageBox::warning(this, "Connection Failed", error);
        return;
    }

    // Health check: prove the link by transmitting a short demo message.
    if (!m_sender->send(DataSender::healthMessage(), error))
    {
        dropSender();
        setLinkDot("red");
        ui->lblLinkStatus->setText("Connection failed");
        updateUiState();
        emit connectionChanged();
        QMessageBox::warning(this, "Connection Failed",
            QString("The link opened but the health-check message could not be transmitted.\n\n%1").arg(error));
        return;
    }

    // Serial only: block briefly until the bytes actually left, so green means
    // "transmitted", not just "queued".
    SerialDataSender* serial = qobject_cast<SerialDataSender*>(m_sender);
    if (serial && !serial->waitForSent(200, error))
    {
        dropSender();
        setLinkDot("red");
        ui->lblLinkStatus->setText("Connection failed");
        updateUiState();
        emit connectionChanged();
        QMessageBox::warning(this, "Connection Failed", error);
        return;
    }

    setLinkDot("green");
    ui->lblLinkStatus->setText(QString("%1 — connected, health message sent").arg(m_sender->description()));
    updateUiState();
    emit connectionChanged();
}

void ConnectionSettingsDialog::onDisconnectClicked()
{
    dropSender();
    setLinkDot("gray");
    ui->lblLinkStatus->setText("Not connected");
    updateUiState();
    emit connectionChanged();
}

void ConnectionSettingsDialog::onSenderLinkError(const QString& message)
{
    setLinkDot("red");
    ui->lblLinkStatus->setText("Link error: " + message);
    // The sender's link is dead (isOpen() now false); keep it until the next
    // Connect/Disconnect so the user can read the reason. The main window stops
    // streaming when it sees connectionChanged().
    updateUiState();
    emit connectionChanged();
}

void ConnectionSettingsDialog::applyDestination(const SimSetup& setup)
{
    ui->cmbDestinationType->setCurrentIndex(setup.destinationType == "TCP" ? 1
                                            : setup.destinationType == "SERIAL" ? 2 : 0);

    ui->txtUdpIp->setText(setup.udpIp);
    ui->spinUdpPort->setValue(setup.udpPort > 0 && setup.udpPort <= 65535 ? setup.udpPort : 5000);
    ui->txtTcpIp->setText(setup.tcpHost);
    ui->spinTcpPort->setValue(setup.tcpPort > 0 && setup.tcpPort <= 65535 ? setup.tcpPort : 5000);

    const int portIndex = ui->cmbSerialPort->findText(setup.serialPortName);
    if (portIndex >= 0)
        ui->cmbSerialPort->setCurrentIndex(portIndex);
    else
        ui->cmbSerialPort->setEditText(setup.serialPortName);

    const QString baudText = QString::number(setup.serialBaud);
    const int baudIndex = ui->cmbSerialBaud->findText(baudText);
    if (baudIndex >= 0)
        ui->cmbSerialBaud->setCurrentIndex(baudIndex);
    else
        ui->cmbSerialBaud->setEditText(baudText);

    const int dataBitsIndex = ui->cmbSerialDataBits->findText(QString::number(setup.serialDataBits));
    ui->cmbSerialDataBits->setCurrentIndex(dataBitsIndex >= 0 ? dataBitsIndex : 0);

    const int parityIndex = ui->cmbSerialParity->findText(setup.serialParity, Qt::MatchFixedString);
    ui->cmbSerialParity->setCurrentIndex(parityIndex >= 0 ? parityIndex : 0);

    const int stopBitsIndex = ui->cmbSerialStopBits->findText(setup.serialStopBits, Qt::MatchFixedString);
    ui->cmbSerialStopBits->setCurrentIndex(stopBitsIndex >= 0 ? stopBitsIndex : 0);
}

void ConnectionSettingsDialog::captureDestination(SimSetup& setup) const
{
    const int destIdx = ui->cmbDestinationType->currentIndex();
    setup.destinationType = (destIdx == 1) ? "TCP" : (destIdx == 2) ? "SERIAL" : "UDP";
    setup.udpIp = ui->txtUdpIp->text().trimmed();
    setup.udpPort = ui->spinUdpPort->value();
    setup.tcpHost = ui->txtTcpIp->text().trimmed();
    setup.tcpPort = ui->spinTcpPort->value();
    setup.serialPortName = ui->cmbSerialPort->currentText().trimmed();
    setup.serialBaud = ui->cmbSerialBaud->currentText().trimmed().toInt();
    setup.serialDataBits = ui->cmbSerialDataBits->currentText().trimmed().toInt();
    setup.serialParity = ui->cmbSerialParity->currentText();
    setup.serialStopBits = ui->cmbSerialStopBits->currentText();
}

#include "SimConnectionsDialog.h"
#include "ui_SimConnectionsDialog.h"

#include "DataSender.h"
#include "SerialDataSender.h"
#include "TcpDataSender.h"
#include "Themes.h"
#include "UdpDataSender.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QStringList>
#include <QTableWidgetItem>

namespace
{
const int COL_NAME = 0;
const int COL_TRANSPORT = 1;
const int COL_DEST = 2;

int transportToIndex(const QString& t)
{
    if (t == "TCP") return 1;
    if (t == "SERIAL") return 2;
    return 0;
}

QString indexToTransport(int i)
{
    if (i == 1) return "TCP";
    if (i == 2) return "SERIAL";
    return "UDP";
}

QString destSummary(const ConnectionDefinition& c)
{
    if (c.transport == "SERIAL")
        return QString("%1 @ %2").arg(c.serialPortName).arg(c.serialBaud);
    return QString("%1:%2").arg(c.host).arg(c.port);
}
}

SimConnectionsDialog::SimConnectionsDialog(QWidget* parent)
    : QDialog(parent),
      ui(new Ui::SimConnectionsDialog),
      m_loading(false)
{
    ui->setupUi(this);
    Themes::apply(this);

    ui->tblConnections->setColumnCount(3);
    ui->tblConnections->setHorizontalHeaderLabels(QStringList()
        << "Name" << "Transport" << "Destination");
    ui->tblConnections->horizontalHeader()->setStretchLastSection(true);
    ui->tblConnections->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblConnections->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tblConnections->setEditTriggers(QAbstractItemView::NoEditTriggers);

    populateSerialPorts(QString());

    connect(ui->btnAddConnection, SIGNAL(clicked()), this, SLOT(onAddConnection()));
    connect(ui->btnRemoveConnection, SIGNAL(clicked()), this, SLOT(onRemoveConnection()));
    connect(ui->tblConnections, SIGNAL(itemSelectionChanged()), this, SLOT(onSelectionChanged()));
    connect(ui->cmbTransport, SIGNAL(currentIndexChanged(int)), this, SLOT(onTransportChanged()));
    connect(ui->btnRefreshSerialPorts, SIGNAL(clicked()), this, SLOT(onRefreshSerialPorts()));
    connect(ui->btnTest, SIGNAL(clicked()), this, SLOT(onTestConnection()));

    connect(ui->txtName, SIGNAL(textEdited(QString)), this, SLOT(onEditorChanged()));
    connect(ui->txtUdpIp, SIGNAL(textEdited(QString)), this, SLOT(onEditorChanged()));
    connect(ui->spinUdpPort, SIGNAL(valueChanged(int)), this, SLOT(onEditorChanged()));
    connect(ui->txtTcpHost, SIGNAL(textEdited(QString)), this, SLOT(onEditorChanged()));
    connect(ui->spinTcpPort, SIGNAL(valueChanged(int)), this, SLOT(onEditorChanged()));
    connect(ui->cmbSerialPort, SIGNAL(editTextChanged(QString)), this, SLOT(onEditorChanged()));
    connect(ui->cmbSerialBaud, SIGNAL(editTextChanged(QString)), this, SLOT(onEditorChanged()));
    connect(ui->cmbSerialDataBits, SIGNAL(currentIndexChanged(int)), this, SLOT(onEditorChanged()));
    connect(ui->cmbSerialParity, SIGNAL(currentIndexChanged(int)), this, SLOT(onEditorChanged()));
    connect(ui->cmbSerialStopBits, SIGNAL(currentIndexChanged(int)), this, SLOT(onEditorChanged()));

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onAccept()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    updateEditorEnabled();
}

SimConnectionsDialog::~SimConnectionsDialog()
{
    delete ui;
}

DataSender* SimConnectionsDialog::buildSender(const ConnectionDefinition& c, QObject* parent)
{
    if (c.transport == "TCP")
    {
        TcpDataSender* tcp = new TcpDataSender(parent);
        tcp->setDestination(c.host, c.port);
        return tcp;
    }
    if (c.transport == "SERIAL")
    {
        SerialDataSender* serial = new SerialDataSender(parent);
        serial->configure(c.serialPortName, c.serialBaud, c.serialDataBits, c.serialParity, c.serialStopBits);
        return serial;
    }
    UdpDataSender* udp = new UdpDataSender(parent);
    udp->setDestination(c.host, c.port);
    return udp;
}

void SimConnectionsDialog::setConnections(const QList<ConnectionDefinition>& connections)
{
    m_connections = connections;
    refreshTable();
    if (!m_connections.isEmpty())
        ui->tblConnections->selectRow(0);
    else
        loadEditor(-1);
}

QList<ConnectionDefinition> SimConnectionsDialog::connections() const
{
    return m_connections;
}

int SimConnectionsDialog::selectedRow() const
{
    const QList<QTableWidgetItem*> sel = ui->tblConnections->selectedItems();
    if (!sel.isEmpty()) return sel.first()->row();
    return ui->tblConnections->currentRow();
}

void SimConnectionsDialog::populateSerialPorts(const QString& keep)
{
    const bool wasLoading = m_loading;
    m_loading = true;
    const QString previous = keep.isEmpty() ? ui->cmbSerialPort->currentText() : keep;
    ui->cmbSerialPort->clear();
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (int i = 0; i < ports.size(); ++i)
        ui->cmbSerialPort->addItem(ports.at(i).portName());
    if (!previous.trimmed().isEmpty())
    {
        const int idx = ui->cmbSerialPort->findText(previous);
        if (idx >= 0) ui->cmbSerialPort->setCurrentIndex(idx);
        else ui->cmbSerialPort->setEditText(previous);
    }
    m_loading = wasLoading;
}

void SimConnectionsDialog::refreshTable()
{
    const bool wasLoading = m_loading;
    m_loading = true;
    ui->tblConnections->setRowCount(0);
    for (int i = 0; i < m_connections.size(); ++i)
    {
        const ConnectionDefinition& c = m_connections.at(i);
        const int row = ui->tblConnections->rowCount();
        ui->tblConnections->insertRow(row);
        ui->tblConnections->setItem(row, COL_NAME, new QTableWidgetItem(c.name));
        ui->tblConnections->setItem(row, COL_TRANSPORT, new QTableWidgetItem(c.transport));
        ui->tblConnections->setItem(row, COL_DEST, new QTableWidgetItem(destSummary(c)));
    }
    ui->tblConnections->resizeColumnsToContents();
    ui->tblConnections->horizontalHeader()->setStretchLastSection(true);
    m_loading = wasLoading;
}

void SimConnectionsDialog::loadEditor(int row)
{
    m_loading = true;
    const bool valid = (row >= 0 && row < m_connections.size());
    if (valid)
    {
        const ConnectionDefinition& c = m_connections.at(row);
        ui->txtName->setText(c.name);
        ui->cmbTransport->setCurrentIndex(transportToIndex(c.transport));
        ui->stackTransport->setCurrentIndex(transportToIndex(c.transport));
        ui->txtUdpIp->setText(c.transport == "UDP" ? c.host : QString());
        ui->spinUdpPort->setValue(c.port > 0 ? c.port : 5000);
        ui->txtTcpHost->setText(c.transport == "TCP" ? c.host : QString());
        ui->spinTcpPort->setValue(c.port > 0 ? c.port : 5000);

        const int portIdx = ui->cmbSerialPort->findText(c.serialPortName);
        if (portIdx >= 0) ui->cmbSerialPort->setCurrentIndex(portIdx);
        else ui->cmbSerialPort->setEditText(c.serialPortName);
        ui->cmbSerialBaud->setEditText(QString::number(c.serialBaud));
        const int dbIdx = ui->cmbSerialDataBits->findText(QString::number(c.serialDataBits));
        ui->cmbSerialDataBits->setCurrentIndex(dbIdx >= 0 ? dbIdx : 0);
        const int paIdx = ui->cmbSerialParity->findText(c.serialParity, Qt::MatchFixedString);
        ui->cmbSerialParity->setCurrentIndex(paIdx >= 0 ? paIdx : 0);
        const int sbIdx = ui->cmbSerialStopBits->findText(c.serialStopBits, Qt::MatchFixedString);
        ui->cmbSerialStopBits->setCurrentIndex(sbIdx >= 0 ? sbIdx : 0);
    }
    else
    {
        ui->txtName->clear();
        ui->cmbTransport->setCurrentIndex(0);
        ui->stackTransport->setCurrentIndex(0);
        ui->txtUdpIp->clear();
        ui->spinUdpPort->setValue(5000);
        ui->txtTcpHost->clear();
        ui->spinTcpPort->setValue(5000);
    }
    ui->lblTestResult->setText("-");
    m_loading = false;
    updateEditorEnabled();
}

void SimConnectionsDialog::updateEditorEnabled()
{
    const bool valid = (selectedRow() >= 0 && selectedRow() < m_connections.size());
    ui->grpEditor->setEnabled(valid);
}

void SimConnectionsDialog::commitEditor()
{
    const int row = selectedRow();
    if (row < 0 || row >= m_connections.size())
        return;

    ConnectionDefinition& c = m_connections[row];
    c.name = ui->txtName->text().trimmed();
    c.transport = indexToTransport(ui->cmbTransport->currentIndex());

    if (c.transport == "UDP")
    {
        c.host = ui->txtUdpIp->text().trimmed();
        c.port = static_cast<quint16>(ui->spinUdpPort->value());
    }
    else if (c.transport == "TCP")
    {
        c.host = ui->txtTcpHost->text().trimmed();
        c.port = static_cast<quint16>(ui->spinTcpPort->value());
    }
    else
    {
        c.serialPortName = ui->cmbSerialPort->currentText().trimmed();
        c.serialBaud = ui->cmbSerialBaud->currentText().trimmed().toInt();
        c.serialDataBits = ui->cmbSerialDataBits->currentText().trimmed().toInt();
        c.serialParity = ui->cmbSerialParity->currentText();
        c.serialStopBits = ui->cmbSerialStopBits->currentText();
    }

    const bool wasLoading = m_loading;
    m_loading = true;
    if (QTableWidgetItem* it = ui->tblConnections->item(row, COL_NAME))
        it->setText(c.name);
    if (QTableWidgetItem* it = ui->tblConnections->item(row, COL_TRANSPORT))
        it->setText(c.transport);
    if (QTableWidgetItem* it = ui->tblConnections->item(row, COL_DEST))
        it->setText(destSummary(c));
    m_loading = wasLoading;
}

void SimConnectionsDialog::onAddConnection()
{
    ConnectionDefinition c;
    c.id = makeConnectionId();
    c.name = QString("Connection %1").arg(m_connections.size() + 1);
    m_connections.append(c);
    refreshTable();
    ui->tblConnections->selectRow(m_connections.size() - 1);
}

void SimConnectionsDialog::onRemoveConnection()
{
    const int row = selectedRow();
    if (row < 0 || row >= m_connections.size())
    {
        QMessageBox::warning(this, "Connections", "Select a connection to remove.");
        return;
    }
    m_connections.removeAt(row);
    refreshTable();
    if (!m_connections.isEmpty())
        ui->tblConnections->selectRow(qMin(row, m_connections.size() - 1));
    else
        loadEditor(-1);
}

void SimConnectionsDialog::onSelectionChanged()
{
    if (m_loading)
        return;
    loadEditor(selectedRow());
}

void SimConnectionsDialog::onTransportChanged()
{
    ui->stackTransport->setCurrentIndex(ui->cmbTransport->currentIndex());
    onEditorChanged();
}

void SimConnectionsDialog::onEditorChanged()
{
    if (m_loading)
        return;
    commitEditor();
}

void SimConnectionsDialog::onRefreshSerialPorts()
{
    populateSerialPorts(QString());
}

void SimConnectionsDialog::onTestConnection()
{
    const int row = selectedRow();
    if (row < 0 || row >= m_connections.size())
        return;
    commitEditor();

    DataSender* sender = buildSender(m_connections.at(row), this);
    QString error;
    if (!sender->open(error) || !sender->send(DataSender::healthMessage(), error))
    {
        ui->lblTestResult->setText(QString("Failed: %1").arg(error));
        sender->close();
        sender->deleteLater();
        return;
    }
    // Serial: confirm the bytes actually left before reporting success.
    SerialDataSender* serial = qobject_cast<SerialDataSender*>(sender);
    if (serial && !serial->waitForSent(200, error))
    {
        ui->lblTestResult->setText(QString("Failed: %1").arg(error));
        sender->close();
        sender->deleteLater();
        return;
    }
    ui->lblTestResult->setText(QString("OK — %1 (health message sent)").arg(sender->description()));
    sender->close();
    sender->deleteLater();
}

void SimConnectionsDialog::onAccept()
{
    QStringList errors;
    for (int i = 0; i < m_connections.size(); ++i)
    {
        ConnectionDefinition& c = m_connections[i];
        if (c.id.trimmed().isEmpty())
            c.id = makeConnectionId();
        if (c.name.trimmed().isEmpty())
            errors << QString("Connection %1 has no name. Solution: give it a label.").arg(i + 1);
        if (c.transport != "SERIAL" && c.host.trimmed().isEmpty())
            errors << QString("Connection '%1' has no destination host/IP. Solution: enter the target address.").arg(c.name);
        if (c.transport == "SERIAL" && c.serialPortName.trimmed().isEmpty())
            errors << QString("Connection '%1' has no COM port. Solution: pick a serial port.").arg(c.name);
    }

    if (!errors.isEmpty())
    {
        QMessageBox box(QMessageBox::Warning, "Connections", errors.join("\n"), QMessageBox::Ok, this);
        if (errors.size() > 4)
            box.setDetailedText(errors.join("\n"));
        box.exec();
        return;
    }
    accept();
}

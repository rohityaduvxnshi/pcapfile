#include "ConfigureConnectionsDialog.h"
#include "ui_ConfigureConnectionsDialog.h"

#include "Themes.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QMessageBox>
#include <QStringList>
#include <QTableWidgetItem>

namespace
{
const int COL_NAME = 0;
const int COL_TRANSPORT = 1;
const int COL_ADAPTER = 2;
const int COL_PORT = 3;
}

ConfigureConnectionsDialog::ConfigureConnectionsDialog(QWidget* parent)
    : QDialog(parent),
      ui(new Ui::ConfigureConnectionsDialog),
      m_loading(false)
{
    ui->setupUi(this);
    Themes::apply(this);

    ui->tblConnections->setColumnCount(4);
    ui->tblConnections->setHorizontalHeaderLabels(QStringList()
        << "Name" << "Transport" << "Adapter" << "Port");
    ui->tblConnections->horizontalHeader()->setStretchLastSection(true);
    ui->tblConnections->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblConnections->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tblConnections->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_adapters = listNetworkAdapters();
    reloadAdapterCombo(QString());

    connect(ui->btnAddConnection, SIGNAL(clicked()), this, SLOT(onAddConnection()));
    connect(ui->btnRemoveConnection, SIGNAL(clicked()), this, SLOT(onRemoveConnection()));
    connect(ui->btnRefreshAdapters, SIGNAL(clicked()), this, SLOT(onRefreshAdapters()));
    connect(ui->tblConnections, SIGNAL(itemSelectionChanged()), this, SLOT(onSelectionChanged()));

    connect(ui->txtName, SIGNAL(textEdited(QString)), this, SLOT(onEditorChanged()));
    connect(ui->cmbTransport, SIGNAL(currentIndexChanged(int)), this, SLOT(onTransportChanged()));
    connect(ui->cmbAdapter, SIGNAL(currentIndexChanged(int)), this, SLOT(onEditorChanged()));
    connect(ui->spinPort, SIGNAL(valueChanged(int)), this, SLOT(onEditorChanged()));
    connect(ui->cmbTcpRole, SIGNAL(currentIndexChanged(int)), this, SLOT(onEditorChanged()));
    connect(ui->txtHost, SIGNAL(textEdited(QString)), this, SLOT(onEditorChanged()));

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(onAccept()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    updateEditorEnabled();
}

ConfigureConnectionsDialog::~ConfigureConnectionsDialog()
{
    delete ui;
}

void ConfigureConnectionsDialog::setConnections(const QList<ConnectionDefinition>& connections)
{
    m_connections = connections;
    refreshTable();
    if (!m_connections.isEmpty())
        ui->tblConnections->selectRow(0);
    else
        loadEditor(-1);
}

QList<ConnectionDefinition> ConfigureConnectionsDialog::connections() const
{
    return m_connections;
}

int ConfigureConnectionsDialog::selectedRow() const
{
    const QList<QTableWidgetItem*> sel = ui->tblConnections->selectedItems();
    if (!sel.isEmpty()) return sel.first()->row();
    return ui->tblConnections->currentRow();
}

void ConfigureConnectionsDialog::reloadAdapterCombo(const QString& selectAddress)
{
    const bool wasLoading = m_loading;
    m_loading = true;
    ui->cmbAdapter->clear();
    for (int i = 0; i < m_adapters.size(); ++i)
        ui->cmbAdapter->addItem(m_adapters.at(i).name);
    const int idx = adapterIndexForAddress(m_adapters, selectAddress);
    if (idx >= 0 && idx < ui->cmbAdapter->count())
        ui->cmbAdapter->setCurrentIndex(idx);
    m_loading = wasLoading;
}

void ConfigureConnectionsDialog::refreshTable()
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
        ui->tblConnections->setItem(row, COL_ADAPTER,
            new QTableWidgetItem(c.adapterName.isEmpty() ? QString("Any adapter") : c.adapterName));
        ui->tblConnections->setItem(row, COL_PORT, new QTableWidgetItem(QString::number(c.port)));
    }
    ui->tblConnections->resizeColumnsToContents();
    ui->tblConnections->horizontalHeader()->setStretchLastSection(true);
    m_loading = wasLoading;
}

void ConfigureConnectionsDialog::loadEditor(int row)
{
    m_loading = true;
    const bool valid = (row >= 0 && row < m_connections.size());
    if (valid)
    {
        const ConnectionDefinition& c = m_connections.at(row);
        ui->txtName->setText(c.name);
        ui->cmbTransport->setCurrentIndex(c.transport == "TCP" ? 1 : 0);
        reloadAdapterCombo(c.adapterAddress);
        ui->spinPort->setValue(c.port);
        ui->cmbTcpRole->setCurrentIndex(c.tcpRole == "Connect" ? 1 : 0);
        ui->txtHost->setText(c.host);
    }
    else
    {
        ui->txtName->clear();
        ui->cmbTransport->setCurrentIndex(0);
        reloadAdapterCombo(QString());
        ui->spinPort->setValue(5000);
        ui->cmbTcpRole->setCurrentIndex(0);
        ui->txtHost->clear();
    }
    m_loading = false;
    updateEditorEnabled();
}

void ConfigureConnectionsDialog::updateEditorEnabled()
{
    const bool valid = (selectedRow() >= 0 && selectedRow() < m_connections.size());
    ui->grpEditor->setEnabled(valid);

    const bool tcp = (ui->cmbTransport->currentIndex() == 1);
    ui->lblTcpRole->setVisible(tcp);
    ui->cmbTcpRole->setVisible(tcp);
    const bool tcpConnect = tcp && (ui->cmbTcpRole->currentIndex() == 1);
    // Host is meaningful for UDP (optional multicast group) and TCP-Connect (remote host).
    ui->txtHost->setEnabled(!tcp || tcpConnect);
    ui->lblHost->setText(tcp ? "Remote host" : "Multicast (optional)");
}

void ConfigureConnectionsDialog::commitEditor()
{
    const int row = selectedRow();
    if (row < 0 || row >= m_connections.size())
        return;

    ConnectionDefinition& c = m_connections[row];
    c.name = ui->txtName->text().trimmed();
    c.transport = (ui->cmbTransport->currentIndex() == 1) ? "TCP" : "UDP";

    const int adapterIdx = ui->cmbAdapter->currentIndex();
    if (adapterIdx >= 0 && adapterIdx < m_adapters.size())
    {
        c.adapterName = m_adapters.at(adapterIdx).isAny ? QString() : m_adapters.at(adapterIdx).name;
        c.adapterAddress = m_adapters.at(adapterIdx).address;
    }
    c.port = static_cast<quint16>(ui->spinPort->value());
    c.tcpRole = (ui->cmbTcpRole->currentIndex() == 1) ? "Connect" : "Listen";
    c.host = ui->txtHost->text().trimmed();

    // Reflect the edit in the list row without disturbing selection.
    const bool wasLoading = m_loading;
    m_loading = true;
    if (QTableWidgetItem* it = ui->tblConnections->item(row, COL_NAME))
        it->setText(c.name);
    if (QTableWidgetItem* it = ui->tblConnections->item(row, COL_TRANSPORT))
        it->setText(c.transport);
    if (QTableWidgetItem* it = ui->tblConnections->item(row, COL_ADAPTER))
        it->setText(c.adapterName.isEmpty() ? QString("Any adapter") : c.adapterName);
    if (QTableWidgetItem* it = ui->tblConnections->item(row, COL_PORT))
        it->setText(QString::number(c.port));
    m_loading = wasLoading;
}

void ConfigureConnectionsDialog::onAddConnection()
{
    ConnectionDefinition c;
    c.id = makeConnectionId();
    c.name = QString("Connection %1").arg(m_connections.size() + 1);
    m_connections.append(c);
    refreshTable();
    ui->tblConnections->selectRow(m_connections.size() - 1);
}

void ConfigureConnectionsDialog::onRemoveConnection()
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

void ConfigureConnectionsDialog::onSelectionChanged()
{
    if (m_loading)
        return;
    loadEditor(selectedRow());
}

void ConfigureConnectionsDialog::onRefreshAdapters()
{
    const int row = selectedRow();
    const QString keep = (row >= 0 && row < m_connections.size())
                             ? m_connections.at(row).adapterAddress : QString();
    m_adapters = listNetworkAdapters();
    reloadAdapterCombo(keep);
}

void ConfigureConnectionsDialog::onEditorChanged()
{
    if (m_loading)
        return;
    commitEditor();
}

void ConfigureConnectionsDialog::onTransportChanged()
{
    updateEditorEnabled();
    onEditorChanged();
}

void ConfigureConnectionsDialog::onAccept()
{
    // Each connection needs a name and a unique transport+adapter+port signature so
    // two receivers never fight over the same bind. Collect all problems into one box.
    QStringList errors;
    for (int i = 0; i < m_connections.size(); ++i)
    {
        const ConnectionDefinition& c = m_connections.at(i);
        if (c.name.trimmed().isEmpty())
            errors << QString("Connection %1 has no name. Solution: give it a label.").arg(i + 1);
        if (c.id.trimmed().isEmpty())
            m_connections[i].id = makeConnectionId();

        for (int j = i + 1; j < m_connections.size(); ++j)
        {
            const ConnectionDefinition& o = m_connections.at(j);
            if (o.transport == c.transport && o.adapterAddress == c.adapterAddress && o.port == c.port)
            {
                errors << QString("Connections '%1' and '%2' bind the same %3 adapter+port (%4). "
                                  "Solution: change the port or adapter on one of them.")
                              .arg(c.name).arg(o.name).arg(c.transport).arg(c.port);
            }
        }
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

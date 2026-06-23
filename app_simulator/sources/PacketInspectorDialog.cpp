#include "PacketInspectorDialog.h"

#include "FieldTypeLabels.h"
#include "PcapWriter.h"
#include "Themes.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace
{
QString hexDump(const QByteArray& data)
{
    QString out;
    const int n = data.size();
    for (int off = 0; off < n; off += 16)
    {
        QString hexPart;
        QString asciiPart;
        for (int i = 0; i < 16; ++i)
        {
            if (off + i < n)
            {
                const quint8 b = static_cast<quint8>(data.at(off + i));
                hexPart += QString("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
                asciiPart += (b >= 0x20 && b < 0x7F) ? QChar(b) : QChar('.');
            }
            else
            {
                hexPart += "   ";
            }
            if (i == 7)
                hexPart += ' ';
        }
        out += QString("%1  %2 %3\n")
                   .arg(off, 4, 16, QChar('0'))
                   .arg(hexPart)
                   .arg(asciiPart);
    }
    return out;
}

QTreeWidgetItem* leaf(QTreeWidgetItem* parent, const QString& text)
{
    QTreeWidgetItem* it = new QTreeWidgetItem(parent);
    it->setText(0, text);
    return it;
}
}

PacketInspectorDialog::PacketInspectorDialog(const QString& transport,
                                             const QString& srcIp, quint16 srcPort,
                                             const QString& dstIp, quint16 dstPort,
                                             const QByteArray& payload,
                                             const QString& messageName,
                                             const MessageDefinition& message,
                                             QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString("Packet — %1").arg(messageName));
    resize(820, 640);

    QVBoxLayout* root = new QVBoxLayout(this);

    const bool tcp = (transport.compare("TCP", Qt::CaseInsensitive) == 0);
    const bool serial = (transport.compare("SERIAL", Qt::CaseInsensitive) == 0);

    QString summary;
    if (serial)
        summary = QString("Serial frame — %1 byte(s). %2").arg(payload.size()).arg(messageName);
    else
        summary = QString("%1  %2:%3 → %4:%5  (%6 payload byte(s))")
                      .arg(tcp ? "TCP" : "UDP")
                      .arg(srcIp).arg(srcPort).arg(dstIp).arg(dstPort).arg(payload.size());
    QLabel* lbl = new QLabel(summary, this);
    lbl->setWordWrap(true);
    root->addWidget(lbl);

    QSplitter* split = new QSplitter(Qt::Vertical, this);

    // 1) Protocol tree.
    QTreeWidget* tree = new QTreeWidget(split);
    tree->setHeaderLabels(QStringList() << "Protocol layer / field");
    tree->setColumnCount(1);

    if (serial)
    {
        QTreeWidgetItem* frame = new QTreeWidgetItem(tree);
        frame->setText(0, QString("Serial data — %1 byte(s) (no network framing)").arg(payload.size()));
    }
    else
    {
        const QByteArray frame = tcp
            ? PcapFrame::buildEthIpTcp(srcIp, srcPort, dstIp, dstPort, payload)
            : PcapFrame::buildEthIpUdp(srcIp, srcPort, dstIp, dstPort, payload);

        QTreeWidgetItem* fr = new QTreeWidgetItem(tree);
        fr->setText(0, QString("Frame: %1 bytes on wire").arg(frame.size()));
        fr->setExpanded(true);

        QTreeWidgetItem* eth = new QTreeWidgetItem(tree);
        eth->setText(0, "Ethernet II");
        eth->setExpanded(true);
        leaf(eth, "Destination: 02:00:00:00:00:02");
        leaf(eth, "Source: 02:00:00:00:00:01");
        leaf(eth, "Type: IPv4 (0x0800)");

        QTreeWidgetItem* ip = new QTreeWidgetItem(tree);
        ip->setText(0, QString("Internet Protocol Version 4, Src: %1, Dst: %2").arg(srcIp, dstIp));
        ip->setExpanded(true);
        leaf(ip, "Version: 4");
        leaf(ip, "Header Length: 20 bytes");
        leaf(ip, QString("Total Length: %1").arg(20 + (tcp ? 20 : 8) + payload.size()));
        leaf(ip, "Time to Live: 64");
        leaf(ip, tcp ? "Protocol: TCP (6)" : "Protocol: UDP (17)");
        leaf(ip, QString("Source: %1").arg(srcIp));
        leaf(ip, QString("Destination: %1").arg(dstIp));

        QTreeWidgetItem* l4 = new QTreeWidgetItem(tree);
        l4->setExpanded(true);
        if (tcp)
        {
            l4->setText(0, QString("Transmission Control Protocol, Src Port: %1, Dst Port: %2")
                               .arg(srcPort).arg(dstPort));
            leaf(l4, QString("Source Port: %1").arg(srcPort));
            leaf(l4, QString("Destination Port: %1").arg(dstPort));
            leaf(l4, "Flags: 0x018 (PSH, ACK)");
            leaf(l4, QString("Payload: %1 byte(s)").arg(payload.size()));
        }
        else
        {
            l4->setText(0, QString("User Datagram Protocol, Src Port: %1, Dst Port: %2")
                               .arg(srcPort).arg(dstPort));
            leaf(l4, QString("Source Port: %1").arg(srcPort));
            leaf(l4, QString("Destination Port: %1").arg(dstPort));
            leaf(l4, QString("Length: %1").arg(8 + payload.size()));
            leaf(l4, "Checksum: 0x0000 (none)");
        }

        QTreeWidgetItem* data = new QTreeWidgetItem(tree);
        data->setText(0, QString("Data (%1 byte(s))").arg(payload.size()));
    }
    split->addWidget(tree);

    // 2) Field breakdown against the message definition.
    QTableWidget* fields = new QTableWidget(split);
    fields->setColumnCount(6);
    fields->setHorizontalHeaderLabels(QStringList()
        << "Field" << "Offset" << "Length" << "Type" << "Bytes (hex)" << "Sent value");
    fields->setEditTriggers(QAbstractItemView::NoEditTriggers);
    fields->verticalHeader()->setVisible(false);
    fields->setSelectionBehavior(QAbstractItemView::SelectRows);
    for (int i = 0; i < message.fields.size(); ++i)
    {
        const FieldDefinition& f = message.fields.at(i);
        const int r = fields->rowCount();
        fields->insertRow(r);
        const int start = f.byteOffsetcorrect;
        QString sliceHex;
        if (start >= 0 && start < payload.size())
            sliceHex = QString::fromLatin1(payload.mid(start, f.length).toHex(' ').toUpper());
        fields->setItem(r, 0, new QTableWidgetItem(f.name));
        fields->setItem(r, 1, new QTableWidgetItem(QString::number(f.byteOffset)));
        fields->setItem(r, 2, new QTableWidgetItem(QString::number(f.length)));
        fields->setItem(r, 3, new QTableWidgetItem(FieldTypeLabels::dataTypeToLabel(f.dataType)));
        fields->setItem(r, 4, new QTableWidgetItem(sliceHex));
        fields->setItem(r, 5, new QTableWidgetItem(f.sendValueText));
    }
    fields->resizeColumnsToContents();
    fields->horizontalHeader()->setStretchLastSection(true);
    split->addWidget(fields);

    // 3) Hex + ASCII dump of the payload.
    QPlainTextEdit* dump = new QPlainTextEdit(split);
    dump->setReadOnly(true);
    QFont mono("Consolas");
    mono.setStyleHint(QFont::Monospace);
    dump->setFont(mono);
    dump->setPlainText(hexDump(payload));
    split->addWidget(dump);

    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    split->setStretchFactor(2, 2);
    root->addWidget(split, 1);

    QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(box, SIGNAL(rejected()), this, SLOT(reject()));
    connect(box, SIGNAL(accepted()), this, SLOT(accept()));
    root->addWidget(box);

    Themes::apply(this);
}

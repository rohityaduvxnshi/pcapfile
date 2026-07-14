#include "CheckableComboBox.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QListView>
#include <QMouseEvent>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QStyleOptionComboBox>
#include <QStylePainter>

CheckableComboBox::CheckableComboBox(QWidget* parent)
    : QComboBox(parent),
      m_model(new QStandardItemModel(this)),
      m_silent(false)
{
    setModel(m_model);

    // A plain QListView + the styled delegate reliably paints the check
    // indicator for every checkable item (the default combo delegate does not).
    QListView* listView = new QListView(this);
    setView(listView);
    listView->setItemDelegate(new QStyledItemDelegate(this));

    // Toggle a row on click without dismissing the popup.
    view()->viewport()->installEventFilter(this);

    connect(m_model, SIGNAL(itemChanged(QStandardItem*)),
            this, SLOT(onModelItemChanged(QStandardItem*)));
}

void CheckableComboBox::clearItems()
{
    m_silent = true;
    m_model->clear();
    m_silent = false;
    update();
}

void CheckableComboBox::addCheckItem(const QString& text, const QString& data, bool checked)
{
    m_silent = true;
    QStandardItem* item = new QStandardItem(text);
    item->setData(data, Qt::UserRole);
    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(checked ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
    m_model->appendRow(item);
    m_silent = false;
    update();
}

QStringList CheckableComboBox::checkedData() const
{
    QStringList out;
    for (int i = 0; i < m_model->rowCount(); ++i)
    {
        QStandardItem* item = m_model->item(i);
        if (item && item->checkState() == Qt::Checked)
            out << item->data(Qt::UserRole).toString();
    }
    return out;
}

void CheckableComboBox::setCheckedData(const QStringList& data)
{
    m_silent = true;
    for (int i = 0; i < m_model->rowCount(); ++i)
    {
        QStandardItem* item = m_model->item(i);
        if (!item)
            continue;
        const bool on = data.contains(item->data(Qt::UserRole).toString());
        item->setCheckState(on ? Qt::Checked : Qt::Unchecked);
    }
    m_silent = false;
    update();
}

QString CheckableComboBox::summaryText() const
{
    if (m_model->rowCount() == 0)
        return QString("No connections");

    QStringList names;
    for (int i = 0; i < m_model->rowCount(); ++i)
    {
        QStandardItem* item = m_model->item(i);
        if (item && item->checkState() == Qt::Checked)
            names << item->text();
    }

    if (names.isEmpty())
        return QString("Default (first)");
    if (names.size() == 1)
        return names.first();
    return QString("%1 selected").arg(names.size());
}

void CheckableComboBox::paintEvent(QPaintEvent*)
{
    // Draw the frame/arrow like a normal combo, but with our multi-select
    // summary as the label instead of a single current item.
    QStylePainter painter(this);
    QStyleOptionComboBox opt;
    initStyleOption(&opt);
    opt.currentText = summaryText();
    painter.drawComplexControl(QStyle::CC_ComboBox, opt);
    painter.drawControl(QStyle::CE_ComboBoxLabel, opt);
}

bool CheckableComboBox::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == view()->viewport() && event->type() == QEvent::MouseButtonRelease)
    {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton)
        {
            const QModelIndex idx = view()->indexAt(me->pos());
            if (idx.isValid())
            {
                QStandardItem* item = m_model->itemFromIndex(idx);
                if (item && (item->flags() & Qt::ItemIsUserCheckable))
                {
                    // Toggle ourselves and consume the event so the popup stays
                    // open (and the delegate does not toggle a second time).
                    item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked
                                                                          : Qt::Checked);
                    return true;
                }
            }
        }
    }
    return QComboBox::eventFilter(watched, event);
}

void CheckableComboBox::onModelItemChanged(QStandardItem*)
{
    update(); // repaint the closed-combo summary
    if (!m_silent)
        emit checkedItemsChanged();
}

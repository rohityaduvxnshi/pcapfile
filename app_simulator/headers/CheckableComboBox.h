#ifndef CHECKABLECOMBOBOX_H
#define CHECKABLECOMBOBOX_H

#include <QComboBox>
#include <QString>
#include <QStringList>

class QStandardItem;
class QStandardItemModel;

// A QComboBox whose popup lists checkable items, so several entries can be
// ticked at once. The closed combo shows a short summary ("2 selected") and the
// popup stays open while you tick/untick. Used for the simulator's per-message
// "send to these connections" multi-binding so one message can fan out to
// several destinations.
//
// It is a QComboBox subclass on purpose: Themes' QSS (dropdown chevron, sizing)
// targets QComboBox, so this widget inherits the app look with no extra styling.
class CheckableComboBox : public QComboBox
{
    Q_OBJECT

public:
    explicit CheckableComboBox(QWidget* parent = 0);

    // Remove every row. Does not emit checkedItemsChanged().
    void clearItems();
    // Append one checkable row. `data` is what checkedData() returns when ticked.
    void addCheckItem(const QString& text, const QString& data, bool checked);

    // The `data` strings of every ticked row, in display order.
    QStringList checkedData() const;
    // Tick exactly the rows whose data is in `data`; untick the rest. Silent.
    void setCheckedData(const QStringList& data);

    // The text drawn on the closed combo for the current selection.
    QString summaryText() const;

signals:
    // Emitted whenever the user ticks/unticks a row (not on programmatic loads).
    void checkedItemsChanged();

protected:
    void paintEvent(QPaintEvent* event) Q_DECL_OVERRIDE;
    bool eventFilter(QObject* watched, QEvent* event) Q_DECL_OVERRIDE;

private slots:
    void onModelItemChanged(QStandardItem* item);

private:
    QStandardItemModel* m_model;
    bool m_silent; // suppress checkedItemsChanged() during programmatic edits
};

#endif // CHECKABLECOMBOBOX_H

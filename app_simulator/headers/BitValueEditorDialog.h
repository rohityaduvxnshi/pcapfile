#ifndef BITVALUEEDITORDIALOG_H
#define BITVALUEEDITORDIALOG_H

// Two-way bit-level value editor for the simulator's field dialog.
//
// The checkbox grid IS the raw wire value (big-endian; byte 1 is transmitted
// first; bit 0 = least significant bit of the last byte). Typing a value in
// the field's own type maps the bits automatically; toggling any bit updates
// the typed value, the raw integer and the hex — all in real time.
//
// Round-trip rule: the typed value shown/returned equals
// PayloadBuilder::typedValueFromRaw(raw) — i.e. raw * resolution — so the
// encode path (value / resolution) reproduces exactly the bits chosen here.

#include "AppTypes.h"

#include <QDialog>
#include <QList>
#include <QString>

class QCheckBox;
class QTableWidgetItem;

namespace Ui
{
class BitValueEditorDialog;
}

class BitValueEditorDialog : public QDialog
{
    Q_OBJECT

public:
    BitValueEditorDialog(const QString& fieldName,
                         FieldDataType dataType,
                         int length,
                         double resolution,
                         const QString& currentValueText,
                         QWidget* parent = 0);
    ~BitValueEditorDialog();

    // The canonical value text to write back into the Value cell.
    QString resultValueText() const;

private slots:
    void onBitToggled(bool checked);
    void onTypedValueEdited(const QString& text);
    void onAddGroupClicked();
    void onRemoveGroupClicked();
    void onGroupCellChanged(QTableWidgetItem* item);
    void onOkClicked();

private:
    // An "advanced bit grouping" row: a named subset of bit positions whose
    // value (dec/hex) maps onto those bits (bits[0] = the value's LSB).
    struct BitGroup
    {
        QString name;
        QList<int> bits;
    };

    quint64 rawFromChecks() const;
    void setChecksFromRaw(quint64 rawValue);
    void updateReadouts(quint64 rawValue);
    void buildBitRows();

    // Advanced bit grouping.
    bool parseBitSpec(const QString& text, QList<int>& bitsOut) const;
    quint64 groupValueFromRaw(const BitGroup& group, quint64 raw) const;
    quint64 applyGroupValue(const BitGroup& group, quint64 value, quint64 raw) const;
    void refreshGroupValues(quint64 raw);   // recompute every group's Dec/Hex
    void appendGroupRow(const BitGroup& group);

    FieldDefinition m_field;            // carries dataType/length/resolution for PayloadBuilder
    QList<QCheckBox*> m_bitChecks;      // indexed by absolute bit number, 0 = LSB
    QList<BitGroup> m_groups;
    bool m_syncing;
    bool m_refreshingGroups;
    QString m_lastReason;               // last typed-value parse failure (for OK)
    QString m_lastSolution;
    QString m_resultValueText;
    Ui::BitValueEditorDialog* ui;
};

#endif // BITVALUEEDITORDIALOG_H

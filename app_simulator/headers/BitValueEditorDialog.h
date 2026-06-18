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
    void onOkClicked();

private:
    quint64 rawFromChecks() const;
    void setChecksFromRaw(quint64 rawValue);
    void updateReadouts(quint64 rawValue);
    void buildBitRows();

    FieldDefinition m_field;            // carries dataType/length/resolution for PayloadBuilder
    QList<QCheckBox*> m_bitChecks;      // indexed by absolute bit number, 0 = LSB
    bool m_syncing;
    QString m_lastReason;               // last typed-value parse failure (for OK)
    QString m_lastSolution;
    QString m_resultValueText;
    Ui::BitValueEditorDialog* ui;
};

#endif // BITVALUEEDITORDIALOG_H

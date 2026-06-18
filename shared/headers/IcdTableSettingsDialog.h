#ifndef ICDTABLESETTINGSDIALOG_H
#define ICDTABLESETTINGSDIALOG_H

#include "IcdImportTypes.h"

#include <QDialog>
#include <QList>
#include <QStringList>

namespace Ui
{
class IcdTableSettingsDialog;
}

// Per-table settings for ICD import (opened from IcdImportDialog's "Selected
// tables" list, never hard-built in MainWindow). Three sections:
//   * Column mapping   - per-table column roles + header row + offset base (with
//                        content-aware auto-detect; saved/loaded as named profiles).
//   * Message identity - per-table message name (blank -> use the ICD heading) and
//                        default port.
//   * Table joining    - tick other selected tables that are continuations of this
//                        one; they merge into this table (the parent). Untick to
//                        unmerge. Children's own Settings are disabled by the caller.
// The dialog only edits/returns data; the caller owns the grouping state.
class IcdTableSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IcdTableSettingsDialog(QWidget* parent = 0);
    ~IcdTableSettingsDialog();

    // Configure for one parent/standalone table. candidateTables are the other
    // selected table indices that may be merged in (parallel candidateLabels for
    // display); currentChildren are those already merged into this table.
    void setContext(const IcdDocument& doc, int tableIndex,
                    const IcdMappingProfile& mapping,
                    const QList<int>& candidateTables,
                    const QStringList& candidateLabels,
                    const QList<int>& currentChildren);

    IcdMappingProfile mapping() const;   // the edited per-table mapping/identity
    QList<int> mergedChildren() const;   // candidate tables ticked to merge in

private slots:
    void onAutoDetectClicked();
    void onNameSourceChanged();
    void onUnmergeAllClicked();
    void onSaveProfileClicked();
    void onLoadProfileClicked();
    void fillCombosForTable();   // also a slot: refills when the header row changes

private:
    void applyMappingToUi(const IcdMappingProfile& m);
    IcdMappingProfile collectMappingFromUi() const;
    QStringList headerCells() const;

    IcdDocument m_doc;
    int m_tableIndex;
    QList<int> m_candidateTables;
    Ui::IcdTableSettingsDialog* ui;
};

#endif // ICDTABLESETTINGSDIALOG_H

#ifndef ICDIMPORTDIALOG_H
#define ICDIMPORTDIALOG_H

#include "IcdImportTypes.h"

#include <QDialog>
#include <QHash>
#include <QList>

class QPushButton;

namespace Ui
{
class IcdImportDialog;
}

// Review/selection dialog for ICD .docx import. Three stages, all driven from the
// .ui form (forms/IcdImportDialog.ui):
//   1. Tables found     - tick the tables that hold field definitions.
//   2. Selected tables  - one row per ticked table with a per-table Settings button
//                         (opens IcdTableSettingsDialog: column mapping + message
//                         identity + table joining). Likely continuation tables are
//                         pre-merged; a merged child shows its parent and a disabled
//                         Settings (configure it from the parent).
//   3. Build & review   - Build assembles one message per group (parent + merged
//                         children), shown in a review tree with a per-message
//                         Preview button; tick what to import; OK validates.
// Grouping state lives here (m_parentOf + per-table m_tableMapping); the settings
// dialog only edits data and the importer (IcdDocxImporter::buildGroupedDrafts)
// does the field stitching.
class IcdImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IcdImportDialog(QWidget* parent = 0);
    ~IcdImportDialog();

    void setDocument(const IcdDocument& doc);
    QList<MessageDefinition> selectedMessages() const;

private slots:
    void onTableSelectionChanged();
    void on_btnCheckAllTables_clicked();
    void on_btnUncheckAllTables_clicked();
    void onTableSettingsClicked();
    void onBuildClicked();
    void onPreviewClicked();
    void onCheckAll();
    void onUncheckAll();
    void onAccept();

private:
    void populateTableList();
    void refreshSelectedTablesTable();
    void populateReviewTree();
    QList<IcdTableGroup> buildGroups() const;
    QString tableLabel(int tableIndex) const;
    QList<int> childrenOf(int parentIndex) const;
    QList<int> candidateChildrenFor(int parentIndex) const;
    void openSettingsForTable(int tableIndex);
    void previewGroup(int parentIndex);

    IcdDocument m_doc;
    QList<IcdMessageDraft> m_drafts;
    QList<MessageDefinition> m_result;

    QList<int> m_selectedTables;                  // ticked table indices, document order
    QHash<int, int> m_parentOf;                   // table index -> parent index (self = standalone)
    QHash<int, IcdMappingProfile> m_tableMapping; // per-table column mapping + identity
    bool m_autoSeeded;                            // continuation auto-grouping done once

    Ui::IcdImportDialog* ui;
};

#endif // ICDIMPORTDIALOG_H

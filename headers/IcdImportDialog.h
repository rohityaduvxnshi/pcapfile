#ifndef ICDIMPORTDIALOG_H
#define ICDIMPORTDIALOG_H

#include "IcdImportTypes.h"

#include <QDialog>
#include <QList>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTreeWidget;

// Review/selection dialog for ICD .docx import. Built programmatically (no .ui)
// because the column-mapping combos and review tree are populated dynamically
// from the parsed document. Flow:
//   1. setDocument() with the Stage-1 extraction.
//   2. User marks which tables are field tables and maps columns -> field roles.
//   3. "Build / Preview" applies the mapping and fills the review tree.
//   4. User ticks the messages/fields to keep, optionally edits per-message
//      port / payload length / optional-header.
//   5. OK validates and exposes the kept messages via selectedMessages().
// Column mappings can be saved/loaded as named profiles to make repeat imports
// of same-shaped ICDs a single click.
class IcdImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IcdImportDialog(QWidget* parent = 0);

    void setDocument(const IcdDocument& doc);
    QList<MessageDefinition> selectedMessages() const;

private slots:
    void onReferenceTableChanged();
    void onAutoDetectClicked();
    void onNameSourceChanged();
    void onBuildClicked();
    void onCheckAll();
    void onUncheckAll();
    void onSaveProfileClicked();
    void onLoadProfileClicked();
    void onAccept();

private:
    void buildUi();
    void populateTableList();
    void repopulateColumnCombos();
    int referenceTableIndex() const;
    QList<int> checkedTableIndices() const;
    IcdMappingProfile currentProfileFromUi() const;
    void applyProfileToUi(const IcdMappingProfile& profile);
    void autoDetectMapping(int tableIndex);
    void populateReviewTree();
    QStringList referenceHeaderCells() const;

    IcdDocument m_doc;
    QList<IcdMessageDraft> m_drafts;
    QList<MessageDefinition> m_result;

    QListWidget*    m_lstTables;
    QSpinBox*       m_spnHeaderRow;
    QComboBox*      m_cmbOffsetBase;
    QLabel*         m_lblColumnsFor;
    QComboBox*      m_cmbColName;
    QComboBox*      m_cmbColOffset;
    QComboBox*      m_cmbColType;
    QComboBox*      m_cmbColLength;
    QComboBox*      m_cmbColResolution;
    QComboBox*      m_cmbColExpr;
    QComboBox*      m_cmbNameSource;
    QLineEdit*      m_txtNamePrefix;
    QSpinBox*       m_spnDefaultPort;
    QCheckBox*      m_chkAutoLength;
    QPushButton*    m_btnAutoDetect;
    QTreeWidget*    m_tree;
    QPlainTextEdit* m_txtWarnings;
    int             m_autoDetectedForTable;   // last table auto-detect ran for (-2 = none)
};

#endif // ICDIMPORTDIALOG_H

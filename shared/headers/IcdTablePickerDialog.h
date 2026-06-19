#ifndef ICDTABLEPICKERDIALOG_H
#define ICDTABLEPICKERDIALOG_H

#include "IcdImportTypes.h"

#include <QDialog>
#include <QList>

class QUrl;

namespace Ui
{
class IcdTablePickerDialog;
}

class IcdTablePickerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IcdTablePickerDialog(QWidget* parent = 0);
    ~IcdTablePickerDialog();

    void setDocument(const IcdDocument& doc);
    void setPreselected(const QList<int>& tableIndices);
    QList<int> selectedTables() const;

private slots:
    void onCheckAll();
    void onUncheckAll();
    void onTogglePreview();
    void onTableClicked(int row, int column);
    void onPreviewAnchorClicked(const QUrl& url);
    void onItemChanged();

private:
    void populateTableList();
    void buildPreviewHtml();
    void scrollPreviewToTable(int tableIndex);
    void updateSummary();
    QString tableLabel(int tableIndex) const;

    IcdDocument m_doc;
    bool m_populating;
    Ui::IcdTablePickerDialog* ui;
};

#endif // ICDTABLEPICKERDIALOG_H

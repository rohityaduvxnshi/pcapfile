#include "IcdImportDialog.h"
#include "ui_IcdImportDialog.h"

#include <QListWidget>
#include <QListWidgetItem>

namespace
{
void setAllTableChecks(QListWidget* list, Qt::CheckState state)
{
    if (!list)
        return;

    list->blockSignals(true);
    for (int i = 0; i < list->count(); ++i)
    {
        QListWidgetItem* item = list->item(i);
        if (item)
            item->setCheckState(state);
    }
    list->blockSignals(false);
}
}

void IcdImportDialog::on_btnCheckAllTables_clicked()
{
    setAllTableChecks(ui->lstTables, Qt::Checked);
    onTableSelectionChanged();
}

void IcdImportDialog::on_btnUncheckAllTables_clicked()
{
    setAllTableChecks(ui->lstTables, Qt::Unchecked);
    onTableSelectionChanged();
}

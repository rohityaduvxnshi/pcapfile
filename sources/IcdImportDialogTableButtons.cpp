#include "IcdImportDialog.h"
#include "ui_IcdImportDialog.h"

#include <QTableWidget>
#include <QTableWidgetItem>

namespace
{
// Box 1 is a QTableWidget (checkable label in column 0, Preview button in
// column 1); ticking state lives on the column-0 items.
void setAllTableChecks(QTableWidget* table, Qt::CheckState state)
{
    if (!table)
        return;

    table->blockSignals(true);
    for (int i = 0; i < table->rowCount(); ++i)
    {
        QTableWidgetItem* item = table->item(i, 0);
        if (item)
            item->setCheckState(state);
    }
    table->blockSignals(false);
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

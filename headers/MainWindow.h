#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "AppTypes.h"

#include <QMainWindow>
#include <QStringList>

namespace Ui
{
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();

private slots:
    void onBrowseClicked();
    void onAddFieldClicked();
    void onRemoveFieldClicked();
    void onStartClicked();

private:
    QString tableText(int row, int column) const;
    bool collectFields(QList<FieldDefinition>& fields, QString& errorMessage) const;
    QStringList buildOutputHeaders(const QList<FieldDefinition>& fields) const;
    void prepareOutputTable(const QStringList& headers);
    void appendPreviewRow(const QStringList& row);
    void setBusy(bool busy);
    void setStatus(const QString& message);

    Ui::MainWindow* ui;
    static const int PREVIEW_ROW_LIMIT = 5000;
};

#endif // MAINWINDOW_H

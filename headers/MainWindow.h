#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "AppTypes.h"
#include "FilterTypes.h"

#include <QList>
#include <QMainWindow>
#include <QStringList>

class QLineEdit;
class QSpinBox;

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
    void onBitfieldDecoderClicked();
    void onStartClicked();
    void onFilterCountChanged(int count);
    void onFilterModeChanged();

private:
    QString tableText(int row, int column) const;
    bool collectFields(QList<FieldDefinition>& fields, QString& errorMessage) const;
    bool collectFilterConfiguration(FilterConfiguration& config, QString& errorMessage) const;

    QStringList buildOutputHeaders(const QList<FieldDefinition>& fields) const;
    QStringList buildPreviewHeaders(const QList<FieldDefinition>& fields) const;
    void prepareOutputTable(const QStringList& headers);
    void appendPreviewRow(const QStringList& row);

    void rebuildFilterInputs();
    void clearPortFilterBoxes();
    void clearHeaderFilterBoxes();
    int matchingFilterIndex(const ParsedUdpPacket& parsed, const FilterConfiguration& config) const;

    QString buildPartitionCsvPath(const QString& baseCsvPath,
                                  const QString& modeText,
                                  const QString& filterLabel) const;

    void setBusy(bool busy);
    void setStatus(const QString& message);

    Ui::MainWindow* ui;
    QList<QSpinBox*> m_portFilterBoxes;
    QList<QLineEdit*> m_headerFilterBoxes;

    static const int PREVIEW_ROW_LIMIT = 5000;
};

#endif // MAINWINDOW_H

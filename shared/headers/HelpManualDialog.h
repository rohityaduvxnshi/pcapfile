#ifndef HELPMANUALDIALOG_H
#define HELPMANUALDIALOG_H

// Shared, searchable in-app user-manual viewer used by both apps in the suite.
// Loads an HTML manual embedded in the app's Qt resources (generated from the
// markdown source), shows a section list (table of contents) built from the
// HTML headings, and supports find-next/prev with wrap-around so users can look
// up common functions and troubleshooting from inside the software.

#include <QDialog>
#include <QString>

class QListWidgetItem;

namespace Ui
{
class HelpManualDialog;
}

class HelpManualDialog : public QDialog
{
    Q_OBJECT

public:
    // htmlResourcePath e.g. ":/manual/parser_manual.html" (the images it references
    // must live under the same resource prefix).
    explicit HelpManualDialog(const QString& htmlResourcePath,
                              const QString& windowTitle,
                              QWidget* parent = 0);
    ~HelpManualDialog();

private slots:
    void onFindNext();
    void onFindPrev();
    void onTocItemActivated(QListWidgetItem* item);

private:
    void loadManual(const QString& htmlResourcePath);
    void buildTableOfContents(const QString& html);
    void runFind(bool backward);

    Ui::HelpManualDialog* ui;
};

#endif // HELPMANUALDIALOG_H

#include "HelpManualDialog.h"
#include "ui_HelpManualDialog.h"

#include "Themes.h"

#include <QFile>
#include <QKeySequence>
#include <QListWidgetItem>
#include <QRegularExpression>
#include <QShortcut>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>

HelpManualDialog::HelpManualDialog(const QString& htmlResourcePath,
                                   const QString& windowTitle,
                                   QWidget* parent)
    : QDialog(parent),
      ui(new Ui::HelpManualDialog)
{
    ui->setupUi(this);
    Themes::apply(this);
    setWindowTitle(windowTitle);

    ui->txtManual->setOpenExternalLinks(true);

    // The manual always renders as a fixed light "document" page (white paper,
    // dark ink), independent of the app's Light/Dark theme — otherwise the dark
    // theme's QSS would paint dark text on a dark widget and the page would be
    // unreadable. The document margin gives the text breathing room.
    ui->txtManual->setStyleSheet(
        "QTextBrowser{background:#FFFFFF;color:#1F2933;border:1px solid #E5E7EB;border-radius:8px;}");
    ui->txtManual->document()->setDocumentMargin(18);

    connect(ui->btnFindNext, SIGNAL(clicked()), this, SLOT(onFindNext()));
    connect(ui->btnFindPrev, SIGNAL(clicked()), this, SLOT(onFindPrev()));
    connect(ui->txtSearch, SIGNAL(returnPressed()), this, SLOT(onFindNext()));
    connect(ui->lstToc, SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(onTocItemActivated(QListWidgetItem*)));
    connect(ui->lstToc, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(onTocItemActivated(QListWidgetItem*)));
    connect(ui->btnBack, SIGNAL(clicked()), ui->txtManual, SLOT(backward()));
    connect(ui->btnForward, SIGNAL(clicked()), ui->txtManual, SLOT(forward()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    // Ctrl+F focuses the search box; F3 / Shift+F3 step through matches.
    QShortcut* scFind = new QShortcut(QKeySequence::Find, this);
    connect(scFind, SIGNAL(activated()), ui->txtSearch, SLOT(setFocus()));
    QShortcut* scNext = new QShortcut(QKeySequence(Qt::Key_F3), this, SLOT(onFindNext()));
    Q_UNUSED(scNext);
    QShortcut* scPrev = new QShortcut(QKeySequence("Shift+F3"), this, SLOT(onFindPrev()));
    Q_UNUSED(scPrev);

    loadManual(htmlResourcePath);
}

HelpManualDialog::~HelpManualDialog()
{
    delete ui;
}

void HelpManualDialog::loadManual(const QString& htmlResourcePath)
{
    // Read the HTML once for the table of contents, then let QTextBrowser load it
    // via its resource URL so relative <img> sources and #anchors resolve.
    QFile file(htmlResourcePath);
    QString html;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        html = QString::fromUtf8(file.readAll());
        file.close();
    }

    if (html.isEmpty())
    {
        ui->txtManual->setHtml(QString(
            "<h2>Manual not available</h2>"
            "<p>The bundled manual could not be loaded from <code>%1</code>.</p>")
            .arg(htmlResourcePath.toHtmlEscaped()));
        return;
    }

    // ":/manual/x.html" -> "qrc:/manual/x.html" so the browser resolves it.
    QString url = htmlResourcePath;
    if (url.startsWith(':'))
        url = "qrc" + url;
    ui->txtManual->setSource(QUrl(url));

    buildTableOfContents(html);
}

void HelpManualDialog::buildTableOfContents(const QString& html)
{
    ui->lstToc->clear();
    // Headings carry an id (emitted by the markdown->HTML generator):
    // <h2 id="common-functions"> ... </h2>
    QRegularExpression re("<h([1-3])[^>]*\\bid=\"([^\"]+)\"[^>]*>(.*?)</h[1-3]>",
                          QRegularExpression::CaseInsensitiveOption
                              | QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression tagStrip("<[^>]+>");
    QRegularExpressionMatchIterator it = re.globalMatch(html);
    while (it.hasNext())
    {
        const QRegularExpressionMatch m = it.next();
        const int level = m.captured(1).toInt();
        const QString anchor = m.captured(2);
        QString title = m.captured(3);
        title.remove(tagStrip);
        title = title.simplified();
        if (title.isEmpty())
            continue;
        const QString indent = (level >= 3) ? "      " : (level == 2 ? "   " : "");
        QListWidgetItem* item = new QListWidgetItem(indent + title, ui->lstToc);
        item->setData(Qt::UserRole, anchor);
        if (level == 1)
        {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        }
    }
}

void HelpManualDialog::onTocItemActivated(QListWidgetItem* item)
{
    if (!item)
        return;
    const QString anchor = item->data(Qt::UserRole).toString();
    if (!anchor.isEmpty())
        ui->txtManual->scrollToAnchor(anchor);
}

void HelpManualDialog::onFindNext()
{
    runFind(false);
}

void HelpManualDialog::onFindPrev()
{
    runFind(true);
}

void HelpManualDialog::runFind(bool backward)
{
    const QString term = ui->txtSearch->text();
    if (term.isEmpty())
    {
        ui->lblFindStatus->clear();
        return;
    }

    QTextDocument::FindFlags flags;
    if (backward)
        flags |= QTextDocument::FindBackward;

    bool found = ui->txtManual->find(term, flags);
    if (!found)
    {
        // Wrap around to the start (or end) and try once more.
        QTextCursor cursor = ui->txtManual->textCursor();
        cursor.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
        ui->txtManual->setTextCursor(cursor);
        found = ui->txtManual->find(term, flags);
    }

    ui->lblFindStatus->setText(found ? QString("Found '%1'").arg(term)
                                     : QString("Not found: '%1'").arg(term));
}

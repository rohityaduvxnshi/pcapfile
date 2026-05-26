#ifndef THEMES_H
#define THEMES_H

#include <QString>

class QWidget;

// v12: Centralized dark / light theme stylesheets + a runtime toggle.
//
// Background: prior to v12 each .ui form had its own dark stylesheet baked into
// the styleSheet property. v12 adds a Themes layer so the app can switch to a
// light palette at runtime. Each window / dialog constructor calls
// Themes::apply(this) after setupUi(this); that single appended line overwrites
// the form's baked-in dark stylesheet with whichever theme the user currently
// has selected (default: dark, matching pre-v12 behaviour).
//
// Persistence: the selected theme is stored in QSettings under "ui/theme"
// (values: "dark" or "light"). Default = "dark".
class Themes
{
public:
    enum Mode { Dark = 0, Light = 1 };

    static Mode currentMode();
    static void setMode(Mode mode);

    static QString currentStyleSheet();
    static QString darkStyleSheet();
    static QString lightStyleSheet();

    // Apply the current theme to a single widget (typically: a top-level
    // window or dialog, called right after setupUi).
    static void apply(QWidget* widget);

    // Walk every top-level widget owned by QApplication and re-apply the
    // current theme. Used after a runtime toggle so already-open dialogs
    // pick up the new palette.
    static void applyToAllTopLevels();
};

#endif // THEMES_H

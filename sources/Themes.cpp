#include "Themes.h"

#include <QApplication>
#include <QSettings>
#include <QWidget>
#include <QWidgetList>

namespace
{
const char* SETTINGS_KEY = "ui/theme";

// Dark theme — kept for users who prefer it, but softened: regular font weight,
// rounder corners, calmer contrast, and a styled, always-readable QToolTip.
const char* DARK_STYLE =
    "QWidget{font-family:\"Segoe UI\",\"Bahnschrift\",Arial;font-size:10pt;font-weight:400;color:#d7e0f4;background-color:#11182a;}"
    "QMainWindow,QDialog{background-color:#11182a;}"
    "QGroupBox{background-color:#18213a;border:1px solid #2a3a5e;border-radius:8px;margin-top:16px;padding:8px 6px;}"
    "QGroupBox::title{subcontrol-origin:margin;subcontrol-position:top left;padding:2px 10px;color:#7fc4ff;background-color:#1f2c4d;border-radius:6px;font-weight:600;}"
    "QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox,QPlainTextEdit,QTextEdit{background-color:#141c33;border:1px solid #2a3a5e;border-radius:6px;padding:5px 8px;color:#d7e0f4;selection-background-color:#2d4a82;}"
    "QLineEdit:focus,QSpinBox:focus,QDoubleSpinBox:focus,QComboBox:focus{border:1px solid #4c8dff;}"
    "QComboBox::drop-down{border:none;width:18px;}"
    "QPushButton,QToolButton{background-color:#1d2a4a;border:1px solid #36558f;border-radius:6px;padding:6px 14px;color:#9ecbff;font-weight:600;}"
    "QPushButton:hover,QToolButton:hover{background-color:#27406f;color:#ffffff;}"
    "QPushButton:pressed,QToolButton:pressed{background-color:#16243f;}"
    "QPushButton:disabled,QToolButton:disabled{background-color:#141c33;color:#3d4c6e;border-color:#243353;}"
    "QToolButton::menu-indicator{image:none;}"
    "QTableWidget,QTreeWidget,QListWidget{background-color:#141c33;alternate-background-color:#121930;gridline-color:#22304f;border:1px solid #2a3a5e;border-radius:6px;selection-background-color:#2d4a82;selection-color:#eef4ff;}"
    "QHeaderView::section{background-color:#1f2c4d;color:#8fa6cc;border:0px;border-right:1px solid #2a3a5e;border-bottom:1px solid #2a3a5e;padding:5px 7px;font-weight:600;font-size:9pt;}"
    "QProgressBar{border:1px solid #2a3a5e;border-radius:6px;background-color:#141c33;text-align:center;color:#d7e0f4;}"
    "QProgressBar::chunk{background-color:#3d77d8;border-radius:5px;}"
    "QScrollBar:vertical{background:#11182a;width:8px;border-radius:4px;}"
    "QScrollBar::handle:vertical{background:#2a3a5e;border-radius:4px;min-height:24px;}"
    "QScrollBar::handle:vertical:hover{background:#4c8dff;}"
    "QScrollBar:horizontal{background:#11182a;height:8px;}"
    "QScrollBar::handle:horizontal{background:#2a3a5e;border-radius:4px;min-width:24px;}"
    "QScrollBar::handle:horizontal:hover{background:#4c8dff;}"
    "QScrollBar::add-line,QScrollBar::sub-line{width:0;height:0;}"
    "QMenuBar{background-color:#11182a;color:#d7e0f4;}"
    "QMenuBar::item{padding:4px 10px;}"
    "QMenuBar::item:selected{background-color:#1f2c4d;border-radius:4px;}"
    "QMenu{background-color:#18213a;border:1px solid #2a3a5e;color:#d7e0f4;}"
    "QMenu::item{padding:5px 24px 5px 12px;}"
    "QMenu::item:selected{background-color:#2d4a82;}"
    "QStatusBar{background-color:#11182a;color:#8fa6cc;border-top:1px solid #2a3a5e;}"
    "QRadioButton,QCheckBox{spacing:7px;color:#d7e0f4;}"
    "QRadioButton::indicator{width:15px;height:15px;border-radius:8px;border:1px solid #36558f;background-color:#141c33;}"
    "QRadioButton::indicator:checked{background-color:#4c8dff;border-color:#4c8dff;}"
    "QCheckBox::indicator{width:15px;height:15px;border-radius:4px;border:1px solid #36558f;background-color:#141c33;}"
    "QCheckBox::indicator:checked{background-color:#4c8dff;border-color:#4c8dff;}"
    "QToolTip{color:#eaf2ff;background-color:#243353;border:1px solid #4c8dff;padding:6px 8px;font-size:9pt;font-weight:400;}"
    "QLabel#lblStatus,QLabel#lblHeaderFieldStatus,QLabel#lblLiveFieldStatus,QLabel#lblInfo,QLabel#lblHeading{color:#7fc4ff;background-color:#18213a;border:1px solid #2a3a5e;border-radius:6px;padding:6px;}";

// Light theme — the default. Soft neutral background, regular-weight Segoe UI,
// rounded friendly controls, gentle blue accents, readable tooltips.
const char* LIGHT_STYLE =
    "QWidget{font-family:\"Segoe UI\",Arial;font-size:10pt;font-weight:400;color:#2b3548;background-color:#f6f8fb;}"
    "QMainWindow,QDialog{background-color:#f6f8fb;}"
    "QGroupBox{background-color:#ffffff;border:1px solid #dde5f0;border-radius:8px;margin-top:16px;padding:8px 6px;}"
    "QGroupBox::title{subcontrol-origin:margin;subcontrol-position:top left;padding:2px 10px;color:#2f6fdb;background-color:#eaf2ff;border-radius:6px;font-weight:600;}"
    "QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox,QPlainTextEdit,QTextEdit{background-color:#ffffff;border:1px solid #cfdaea;border-radius:6px;padding:5px 8px;color:#2b3548;selection-background-color:#cfe3ff;}"
    "QLineEdit:focus,QSpinBox:focus,QDoubleSpinBox:focus,QComboBox:focus{border:1px solid #4c8dff;}"
    "QComboBox::drop-down{border:none;width:18px;}"
    "QPushButton,QToolButton{background-color:#eef4ff;border:1px solid #bcd4ff;border-radius:6px;padding:6px 14px;color:#1f5fd0;font-weight:600;}"
    "QPushButton:hover,QToolButton:hover{background-color:#dcebff;color:#0d47a1;}"
    "QPushButton:pressed,QToolButton:pressed{background-color:#c8e0ff;}"
    "QPushButton:disabled,QToolButton:disabled{background-color:#f0f2f5;color:#aeb8c8;border-color:#dfe5ee;}"
    "QToolButton::menu-indicator{image:none;}"
    "QTableWidget,QTreeWidget,QListWidget{background-color:#ffffff;alternate-background-color:#f4f7fc;gridline-color:#e8edf5;border:1px solid #dde5f0;border-radius:6px;selection-background-color:#cfe3ff;selection-color:#103e91;}"
    "QHeaderView::section{background-color:#f0f4fa;color:#46556b;border:0px;border-right:1px solid #e1e8f2;border-bottom:1px solid #e1e8f2;padding:5px 7px;font-weight:600;font-size:9pt;}"
    "QProgressBar{border:1px solid #dde5f0;border-radius:6px;background-color:#ffffff;text-align:center;color:#2b3548;}"
    "QProgressBar::chunk{background-color:#4c8dff;border-radius:5px;}"
    "QScrollBar:vertical{background:#f6f8fb;width:8px;border-radius:4px;}"
    "QScrollBar::handle:vertical{background:#cfdaea;border-radius:4px;min-height:24px;}"
    "QScrollBar::handle:vertical:hover{background:#4c8dff;}"
    "QScrollBar:horizontal{background:#f6f8fb;height:8px;}"
    "QScrollBar::handle:horizontal{background:#cfdaea;border-radius:4px;min-width:24px;}"
    "QScrollBar::handle:horizontal:hover{background:#4c8dff;}"
    "QScrollBar::add-line,QScrollBar::sub-line{width:0;height:0;}"
    "QMenuBar{background-color:#f6f8fb;color:#2b3548;}"
    "QMenuBar::item{padding:4px 10px;}"
    "QMenuBar::item:selected{background-color:#dcebff;border-radius:4px;}"
    "QMenu{background-color:#ffffff;border:1px solid #dde5f0;color:#2b3548;}"
    "QMenu::item{padding:5px 24px 5px 12px;}"
    "QMenu::item:selected{background-color:#dcebff;}"
    "QStatusBar{background-color:#f6f8fb;color:#5a6a82;border-top:1px solid #dde5f0;}"
    "QRadioButton,QCheckBox{spacing:7px;color:#2b3548;}"
    "QRadioButton::indicator{width:15px;height:15px;border-radius:8px;border:1px solid #b7c6dd;background-color:#ffffff;}"
    "QRadioButton::indicator:checked{background-color:#4c8dff;border-color:#4c8dff;}"
    "QCheckBox::indicator{width:15px;height:15px;border-radius:4px;border:1px solid #b7c6dd;background-color:#ffffff;}"
    "QCheckBox::indicator:checked{background-color:#4c8dff;border-color:#4c8dff;}"
    "QToolTip{color:#2b3548;background-color:#fffef5;border:1px solid #d8c98a;padding:6px 8px;font-size:9pt;font-weight:400;}"
    "QLabel#lblStatus,QLabel#lblHeaderFieldStatus,QLabel#lblLiveFieldStatus,QLabel#lblInfo,QLabel#lblHeading{color:#1f5fd0;background-color:#eaf2ff;border:1px solid #cfe0fb;border-radius:6px;padding:6px;}";
}

Themes::Mode Themes::currentMode()
{
    // Default flipped to Light (user request: a lighter, friendlier first
    // impression). Anyone who has toggled before keeps their stored choice.
    QSettings settings;
    const QString value = settings.value(SETTINGS_KEY, QString("light")).toString();
    return (value.compare("light", Qt::CaseInsensitive) == 0) ? Light : Dark;
}

void Themes::setMode(Mode mode)
{
    QSettings settings;
    settings.setValue(SETTINGS_KEY, mode == Light ? QString("light") : QString("dark"));
}

QString Themes::currentStyleSheet()
{
    return currentMode() == Light ? lightStyleSheet() : darkStyleSheet();
}

QString Themes::darkStyleSheet()
{
    return QString::fromLatin1(DARK_STYLE);
}

QString Themes::lightStyleSheet()
{
    return QString::fromLatin1(LIGHT_STYLE);
}

void Themes::apply(QWidget* widget)
{
    if (!widget) return;
    widget->setStyleSheet(currentStyleSheet());
}

void Themes::applyToAllTopLevels()
{
    const QString sheet = currentStyleSheet();
    const QWidgetList tops = QApplication::topLevelWidgets();
    for (int i = 0; i < tops.size(); ++i)
    {
        QWidget* w = tops.at(i);
        if (w)
            w->setStyleSheet(sheet);
    }
}

#include "Themes.h"

#include <QApplication>
#include <QSettings>
#include <QWidget>
#include <QWidgetList>

namespace
{
const char* SETTINGS_KEY = "ui/theme";

const char* DARK_STYLE =
    "QWidget{font-family:\"Bahnschrift\",\"Arial Narrow\",Arial;font-size:11pt;font-weight:600;color:#ccd6f6;background-color:#0a0e1a;}"
    "QMainWindow,QDialog{background-color:#0a0e1a;}"
    "QGroupBox{background-color:#0f1626;border:1px solid #1e2d4a;border-radius:3px;margin-top:14px;padding:6px 4px;}"
    "QGroupBox::title{subcontrol-origin:margin;subcontrol-position:top left;padding:2px 8px;color:#4fc3f7;background-color:#162035;border-radius:2px;font-weight:700;}"
    "QLineEdit,QSpinBox,QComboBox{background-color:#0f1626;border:1px solid #1e2d4a;border-radius:3px;padding:5px 7px;color:#ccd6f6;selection-background-color:#1c3a6e;}"
    "QLineEdit:focus,QSpinBox:focus,QComboBox:focus{border:1px solid #1e88e5;}"
    "QComboBox::drop-down{border:none;}"
    "QPushButton,QToolButton{background-color:#0f1626;border:1px solid #1e88e5;border-radius:3px;padding:5px 12px;color:#4fc3f7;font-weight:700;}"
    "QPushButton:hover,QToolButton:hover{background-color:#1c3a6e;color:#ffffff;}"
    "QPushButton:pressed,QToolButton:pressed{background-color:#1565c0;}"
    "QPushButton:disabled,QToolButton:disabled{background-color:#0a0e1a;color:#2a3a52;border-color:#1e2d4a;}"
    "QToolButton::menu-indicator{image:none;}"
    "QTableWidget{background-color:#0f1626;alternate-background-color:#0c1220;gridline-color:#1a2540;border:1px solid #1e2d4a;border-radius:2px;selection-background-color:#1c3a6e;selection-color:#e8f0fe;}"
    "QHeaderView::section{background-color:#162035;color:#607d8b;border:0px;border-right:1px solid #1e2d4a;border-bottom:1px solid #1e2d4a;padding:4px 6px;font-weight:700;font-size:9pt;}"
    "QProgressBar{border:1px solid #1e2d4a;border-radius:3px;background-color:#0f1626;text-align:center;color:#ccd6f6;}"
    "QProgressBar::chunk{background-color:#1e88e5;border-radius:2px;}"
    "QScrollBar:vertical{background:#0a0e1a;width:6px;border-radius:3px;}"
    "QScrollBar::handle:vertical{background:#1e2d4a;border-radius:3px;}"
    "QScrollBar::handle:vertical:hover{background:#1e88e5;}"
    "QScrollBar:horizontal{background:#0a0e1a;height:6px;}"
    "QScrollBar::handle:horizontal{background:#1e2d4a;border-radius:3px;}"
    "QScrollBar::handle:horizontal:hover{background:#1e88e5;}"
    "QScrollBar::add-line,QScrollBar::sub-line{width:0;height:0;}"
    "QMenuBar{background-color:#0a0e1a;color:#ccd6f6;}"
    "QMenuBar::item:selected{background-color:#162035;}"
    "QMenu{background-color:#0f1626;border:1px solid #1e2d4a;color:#ccd6f6;}"
    "QMenu::item:selected{background-color:#1c3a6e;}"
    "QStatusBar{background-color:#0a0e1a;color:#607d8b;border-top:1px solid #1e2d4a;}"
    "QRadioButton,QCheckBox{spacing:6px;color:#ccd6f6;}"
    "QRadioButton::indicator{width:13px;height:13px;border-radius:7px;border:1px solid #1e2d4a;background-color:#0f1626;}"
    "QRadioButton::indicator:checked{background-color:#1e88e5;border-color:#1e88e5;}"
    "QCheckBox::indicator{width:13px;height:13px;border-radius:2px;border:1px solid #1e2d4a;background-color:#0f1626;}"
    "QCheckBox::indicator:checked{background-color:#1e88e5;border-color:#1e88e5;}"
    "QLabel#lblStatus,QLabel#lblHeaderFieldStatus,QLabel#lblLiveFieldStatus,QLabel#lblInfo,QLabel#lblHeading{color:#4fc3f7;background-color:#0f1626;border:1px solid #1e2d4a;border-radius:2px;padding:5px;}";

const char* LIGHT_STYLE =
    "QWidget{font-family:\"Bahnschrift\",\"Arial Narrow\",Arial;font-size:11pt;font-weight:600;color:#1a2540;background-color:#f5f7fa;}"
    "QMainWindow,QDialog{background-color:#f5f7fa;}"
    "QGroupBox{background-color:#ffffff;border:1px solid #c4d0e1;border-radius:3px;margin-top:14px;padding:6px 4px;}"
    "QGroupBox::title{subcontrol-origin:margin;subcontrol-position:top left;padding:2px 8px;color:#1565c0;background-color:#e3f2fd;border-radius:2px;font-weight:700;}"
    "QLineEdit,QSpinBox,QComboBox{background-color:#ffffff;border:1px solid #c4d0e1;border-radius:3px;padding:5px 7px;color:#1a2540;selection-background-color:#bbdefb;}"
    "QLineEdit:focus,QSpinBox:focus,QComboBox:focus{border:1px solid #1e88e5;}"
    "QComboBox::drop-down{border:none;}"
    "QPushButton,QToolButton{background-color:#ffffff;border:1px solid #1e88e5;border-radius:3px;padding:5px 12px;color:#1565c0;font-weight:700;}"
    "QPushButton:hover,QToolButton:hover{background-color:#bbdefb;color:#0d47a1;}"
    "QPushButton:pressed,QToolButton:pressed{background-color:#90caf9;}"
    "QPushButton:disabled,QToolButton:disabled{background-color:#eceff1;color:#b0bec5;border-color:#cfd8dc;}"
    "QToolButton::menu-indicator{image:none;}"
    "QTableWidget{background-color:#ffffff;alternate-background-color:#f0f4f9;gridline-color:#e0e6ee;border:1px solid #c4d0e1;border-radius:2px;selection-background-color:#bbdefb;selection-color:#0d47a1;}"
    "QHeaderView::section{background-color:#e3f2fd;color:#37474f;border:0px;border-right:1px solid #c4d0e1;border-bottom:1px solid #c4d0e1;padding:4px 6px;font-weight:700;font-size:9pt;}"
    "QProgressBar{border:1px solid #c4d0e1;border-radius:3px;background-color:#ffffff;text-align:center;color:#1a2540;}"
    "QProgressBar::chunk{background-color:#1e88e5;border-radius:2px;}"
    "QScrollBar:vertical{background:#f5f7fa;width:6px;border-radius:3px;}"
    "QScrollBar::handle:vertical{background:#c4d0e1;border-radius:3px;}"
    "QScrollBar::handle:vertical:hover{background:#1e88e5;}"
    "QScrollBar:horizontal{background:#f5f7fa;height:6px;}"
    "QScrollBar::handle:horizontal{background:#c4d0e1;border-radius:3px;}"
    "QScrollBar::handle:horizontal:hover{background:#1e88e5;}"
    "QScrollBar::add-line,QScrollBar::sub-line{width:0;height:0;}"
    "QMenuBar{background-color:#f5f7fa;color:#1a2540;}"
    "QMenuBar::item:selected{background-color:#bbdefb;}"
    "QMenu{background-color:#ffffff;border:1px solid #c4d0e1;color:#1a2540;}"
    "QMenu::item:selected{background-color:#bbdefb;}"
    "QStatusBar{background-color:#f5f7fa;color:#37474f;border-top:1px solid #c4d0e1;}"
    "QRadioButton,QCheckBox{spacing:6px;color:#1a2540;}"
    "QRadioButton::indicator{width:13px;height:13px;border-radius:7px;border:1px solid #c4d0e1;background-color:#ffffff;}"
    "QRadioButton::indicator:checked{background-color:#1e88e5;border-color:#1e88e5;}"
    "QCheckBox::indicator{width:13px;height:13px;border-radius:2px;border:1px solid #c4d0e1;background-color:#ffffff;}"
    "QCheckBox::indicator:checked{background-color:#1e88e5;border-color:#1e88e5;}"
    "QLabel#lblStatus,QLabel#lblHeaderFieldStatus,QLabel#lblLiveFieldStatus,QLabel#lblInfo,QLabel#lblHeading{color:#1565c0;background-color:#e3f2fd;border:1px solid #c4d0e1;border-radius:2px;padding:5px;}";
}

Themes::Mode Themes::currentMode()
{
    QSettings settings;
    const QString value = settings.value(SETTINGS_KEY, QString("dark")).toString();
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

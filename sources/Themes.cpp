#include "Themes.h"

#include <QApplication>
#include <QSettings>
#include <QWidget>
#include <QWidgetList>

namespace
{
const char* SETTINGS_KEY = "ui/theme";

// ---------------------------------------------------------------------------
// Modern Light (primary): near-white surfaces, soft gray hairlines, a single
// calm indigo accent. Flat, airy, minimal. Combo/spin arrows are an embedded
// chevron (assets.qrc) so dropdowns always read as dropdowns.
// ---------------------------------------------------------------------------
const char* LIGHT_STYLE =
    "*{outline:0;}"
    "QWidget{font-family:\"Segoe UI\",\"Inter\",Arial;font-size:10pt;font-weight:400;color:#111827;background-color:#F8FAFC;}"
    "QMainWindow,QDialog{background-color:#F8FAFC;}"
    "QToolTip{color:#F1F5F9;background-color:#1E293B;border:1px solid #334155;border-radius:6px;padding:6px 9px;font-size:9pt;}"
    "QGroupBox{background-color:#FFFFFF;border:1px solid #E5E7EB;border-radius:10px;margin-top:16px;padding:12px 12px 12px 12px;}"
    "QGroupBox::title{subcontrol-origin:margin;subcontrol-position:top left;left:12px;padding:0 4px;color:#4F46E5;font-weight:600;}"
    "QLabel{background:transparent;}"
    "QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox,QPlainTextEdit,QTextEdit{background-color:#FFFFFF;border:1px solid #D1D5DB;border-radius:8px;padding:6px 10px;color:#111827;selection-background-color:#C7D2FE;selection-color:#1E1B4B;}"
    "QLineEdit:hover,QSpinBox:hover,QDoubleSpinBox:hover,QComboBox:hover{border:1px solid #A5B4FC;}"
    "QLineEdit:focus,QSpinBox:focus,QDoubleSpinBox:focus,QComboBox:focus,QPlainTextEdit:focus,QTextEdit:focus{border:1px solid #4F46E5;}"
    "QComboBox::drop-down{subcontrol-origin:padding;subcontrol-position:center right;width:24px;border:none;}"
    "QComboBox::down-arrow{image:url(:/icons/chevron_down.png);width:12px;height:12px;}"
    "QComboBox QAbstractItemView{background:#FFFFFF;border:1px solid #E5E7EB;border-radius:8px;selection-background-color:#E0E7FF;selection-color:#312E81;outline:0;padding:4px;}"
    "QSpinBox::up-button,QDoubleSpinBox::up-button,QSpinBox::down-button,QDoubleSpinBox::down-button{width:18px;border:none;background:transparent;}"
    "QSpinBox::up-arrow,QDoubleSpinBox::up-arrow{image:url(:/icons/chevron_up.png);width:10px;height:10px;}"
    "QSpinBox::down-arrow,QDoubleSpinBox::down-arrow{image:url(:/icons/chevron_down.png);width:10px;height:10px;}"
    "QPushButton{background-color:#FFFFFF;border:1px solid #D1D5DB;border-radius:8px;padding:7px 16px;color:#374151;font-weight:500;}"
    "QPushButton:hover{background-color:#F9FAFB;border-color:#A5B4FC;color:#111827;}"
    "QPushButton:pressed{background-color:#F3F4F6;}"
    "QPushButton:disabled{background-color:#F3F4F6;color:#9CA3AF;border-color:#E5E7EB;}"
    "QPushButton#btnStart,QPushButton#btnStartLive{background-color:#4F46E5;border:1px solid #4F46E5;color:#FFFFFF;font-weight:600;}"
    "QPushButton#btnStart:hover,QPushButton#btnStartLive:hover{background-color:#4338CA;border-color:#4338CA;color:#FFFFFF;}"
    "QPushButton#btnStart:disabled,QPushButton#btnStartLive:disabled{background-color:#C7D2FE;border-color:#C7D2FE;color:#EEF2FF;}"
    "QPushButton#btnStopLive{color:#B91C1C;}"
    "QToolButton{background-color:#FFFFFF;border:1px solid #D1D5DB;border-radius:8px;padding:6px 12px;color:#374151;font-weight:500;}"
    "QToolButton:hover{background-color:#F9FAFB;border-color:#A5B4FC;}"
    "QToolButton::menu-indicator{image:none;}"
    "QTabWidget::pane{border:1px solid #E5E7EB;border-radius:8px;top:-1px;}"
    "QTabBar::tab{background:transparent;color:#6B7280;padding:7px 16px;border:none;border-bottom:2px solid transparent;margin-right:2px;}"
    "QTabBar::tab:hover{color:#4F46E5;}"
    "QTabBar::tab:selected{color:#4F46E5;border-bottom:2px solid #4F46E5;font-weight:600;}"
    "QTableWidget,QTreeWidget,QListWidget,QTableView,QTreeView,QListView{background-color:#FFFFFF;alternate-background-color:#F9FAFB;gridline-color:#EEF1F5;border:1px solid #E5E7EB;border-radius:8px;selection-background-color:#E0E7FF;selection-color:#312E81;}"
    "QTableView::item,QTreeView::item,QListView::item{padding:3px 4px;}"
    "QHeaderView::section{background-color:#F3F4F6;color:#6B7280;border:0;border-right:1px solid #E5E7EB;border-bottom:1px solid #E5E7EB;padding:6px 8px;font-weight:600;font-size:9pt;}"
    "QTableCornerButton::section{background:#F3F4F6;border:0;}"
    "QScrollBar:vertical{background:transparent;width:11px;margin:2px;}"
    "QScrollBar::handle:vertical{background:#CBD5E1;border-radius:5px;min-height:30px;}"
    "QScrollBar::handle:vertical:hover{background:#94A3B8;}"
    "QScrollBar:horizontal{background:transparent;height:11px;margin:2px;}"
    "QScrollBar::handle:horizontal{background:#CBD5E1;border-radius:5px;min-width:30px;}"
    "QScrollBar::handle:horizontal:hover{background:#94A3B8;}"
    "QScrollBar::add-line,QScrollBar::sub-line{width:0;height:0;}"
    "QScrollBar::add-page,QScrollBar::sub-page{background:transparent;}"
    "QScrollArea{background:transparent;border:none;}"
    "QMenuBar{background-color:#F8FAFC;color:#374151;border-bottom:1px solid #E5E7EB;}"
    "QMenuBar::item{padding:5px 12px;background:transparent;}"
    "QMenuBar::item:selected{background-color:#EEF2FF;border-radius:6px;color:#4F46E5;}"
    "QMenu{background-color:#FFFFFF;border:1px solid #E5E7EB;border-radius:8px;color:#111827;padding:4px;}"
    "QMenu::item{padding:6px 26px 6px 14px;border-radius:6px;}"
    "QMenu::item:selected{background-color:#EEF2FF;color:#4F46E5;}"
    "QMenu::separator{height:1px;background:#E5E7EB;margin:4px 8px;}"
    "QStatusBar{background-color:#F8FAFC;color:#6B7280;border-top:1px solid #E5E7EB;}"
    "QCheckBox,QRadioButton{spacing:7px;color:#111827;background:transparent;}"
    "QCheckBox::indicator,QRadioButton::indicator{width:16px;height:16px;border:1px solid #CBD5E1;background:#FFFFFF;}"
    "QCheckBox::indicator{border-radius:4px;}"
    "QRadioButton::indicator{border-radius:9px;}"
    "QCheckBox::indicator:checked,QRadioButton::indicator:checked{background:#4F46E5;border-color:#4F46E5;}"
    "QCheckBox::indicator:hover,QRadioButton::indicator:hover{border-color:#4F46E5;}"
    "QSplitter::handle{background:#E5E7EB;}"
    "QSplitter::handle:vertical{height:6px;}"
    "QSplitter::handle:horizontal{width:6px;}"
    "QProgressBar{border:1px solid #E5E7EB;border-radius:8px;background:#FFFFFF;text-align:center;color:#111827;}"
    "QProgressBar::chunk{background-color:#4F46E5;border-radius:7px;}"
    "QLabel#lblStatus,QLabel#lblHeaderFieldStatus,QLabel#lblLiveFieldStatus,QLabel#lblInfo,QLabel#lblHeading,QLabel#lblLiveStatus{color:#3730A3;background-color:#EEF2FF;border:1px solid #C7D2FE;border-radius:8px;padding:8px 10px;}";

// ---------------------------------------------------------------------------
// Modern Dark (Slate): deep slate base, soft low-glare contrast, single sky
// accent. Same layout language as the light theme.
// ---------------------------------------------------------------------------
const char* DARK_STYLE =
    "*{outline:0;}"
    "QWidget{font-family:\"Segoe UI\",\"Inter\",Arial;font-size:10pt;font-weight:400;color:#E2E8F0;background-color:#0F172A;}"
    "QMainWindow,QDialog{background-color:#0F172A;}"
    "QToolTip{color:#E2E8F0;background-color:#0B1220;border:1px solid #334155;border-radius:6px;padding:6px 9px;font-size:9pt;}"
    "QGroupBox{background-color:#1E293B;border:1px solid #334155;border-radius:10px;margin-top:16px;padding:12px 12px 12px 12px;}"
    "QGroupBox::title{subcontrol-origin:margin;subcontrol-position:top left;left:12px;padding:0 4px;color:#38BDF8;font-weight:600;}"
    "QLabel{background:transparent;}"
    "QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox,QPlainTextEdit,QTextEdit{background-color:#0B1220;border:1px solid #334155;border-radius:8px;padding:6px 10px;color:#E2E8F0;selection-background-color:#1E3A5F;selection-color:#E0F2FE;}"
    "QLineEdit:hover,QSpinBox:hover,QDoubleSpinBox:hover,QComboBox:hover{border:1px solid #475569;}"
    "QLineEdit:focus,QSpinBox:focus,QDoubleSpinBox:focus,QComboBox:focus,QPlainTextEdit:focus,QTextEdit:focus{border:1px solid #38BDF8;}"
    "QComboBox::drop-down{subcontrol-origin:padding;subcontrol-position:center right;width:24px;border:none;}"
    "QComboBox::down-arrow{image:url(:/icons/chevron_down.png);width:12px;height:12px;}"
    "QComboBox QAbstractItemView{background:#1E293B;border:1px solid #334155;border-radius:8px;selection-background-color:#1E3A5F;selection-color:#E0F2FE;outline:0;padding:4px;}"
    "QSpinBox::up-button,QDoubleSpinBox::up-button,QSpinBox::down-button,QDoubleSpinBox::down-button{width:18px;border:none;background:transparent;}"
    "QSpinBox::up-arrow,QDoubleSpinBox::up-arrow{image:url(:/icons/chevron_up.png);width:10px;height:10px;}"
    "QSpinBox::down-arrow,QDoubleSpinBox::down-arrow{image:url(:/icons/chevron_down.png);width:10px;height:10px;}"
    "QPushButton{background-color:#1E293B;border:1px solid #334155;border-radius:8px;padding:7px 16px;color:#CBD5E1;font-weight:500;}"
    "QPushButton:hover{background-color:#243549;border-color:#475569;color:#F1F5F9;}"
    "QPushButton:pressed{background-color:#172033;}"
    "QPushButton:disabled{background-color:#172033;color:#475569;border-color:#243549;}"
    "QPushButton#btnStart,QPushButton#btnStartLive{background-color:#0EA5E9;border:1px solid #0EA5E9;color:#04263A;font-weight:600;}"
    "QPushButton#btnStart:hover,QPushButton#btnStartLive:hover{background-color:#38BDF8;border-color:#38BDF8;color:#04263A;}"
    "QPushButton#btnStart:disabled,QPushButton#btnStartLive:disabled{background-color:#1E3A5F;border-color:#1E3A5F;color:#64748B;}"
    "QPushButton#btnStopLive{color:#F87171;}"
    "QToolButton{background-color:#1E293B;border:1px solid #334155;border-radius:8px;padding:6px 12px;color:#CBD5E1;font-weight:500;}"
    "QToolButton:hover{background-color:#243549;border-color:#475569;}"
    "QToolButton::menu-indicator{image:none;}"
    "QTabWidget::pane{border:1px solid #334155;border-radius:8px;top:-1px;}"
    "QTabBar::tab{background:transparent;color:#94A3B8;padding:7px 16px;border:none;border-bottom:2px solid transparent;margin-right:2px;}"
    "QTabBar::tab:hover{color:#38BDF8;}"
    "QTabBar::tab:selected{color:#38BDF8;border-bottom:2px solid #38BDF8;font-weight:600;}"
    "QTableWidget,QTreeWidget,QListWidget,QTableView,QTreeView,QListView{background-color:#111B2E;alternate-background-color:#0F1A2C;gridline-color:#24324A;border:1px solid #334155;border-radius:8px;selection-background-color:#1E3A5F;selection-color:#E0F2FE;}"
    "QTableView::item,QTreeView::item,QListView::item{padding:3px 4px;}"
    "QHeaderView::section{background-color:#172033;color:#94A3B8;border:0;border-right:1px solid #24324A;border-bottom:1px solid #24324A;padding:6px 8px;font-weight:600;font-size:9pt;}"
    "QTableCornerButton::section{background:#172033;border:0;}"
    "QScrollBar:vertical{background:transparent;width:11px;margin:2px;}"
    "QScrollBar::handle:vertical{background:#334155;border-radius:5px;min-height:30px;}"
    "QScrollBar::handle:vertical:hover{background:#475569;}"
    "QScrollBar:horizontal{background:transparent;height:11px;margin:2px;}"
    "QScrollBar::handle:horizontal{background:#334155;border-radius:5px;min-width:30px;}"
    "QScrollBar::handle:horizontal:hover{background:#475569;}"
    "QScrollBar::add-line,QScrollBar::sub-line{width:0;height:0;}"
    "QScrollBar::add-page,QScrollBar::sub-page{background:transparent;}"
    "QScrollArea{background:transparent;border:none;}"
    "QMenuBar{background-color:#0F172A;color:#CBD5E1;border-bottom:1px solid #334155;}"
    "QMenuBar::item{padding:5px 12px;background:transparent;}"
    "QMenuBar::item:selected{background-color:#1E293B;border-radius:6px;color:#38BDF8;}"
    "QMenu{background-color:#1E293B;border:1px solid #334155;border-radius:8px;color:#E2E8F0;padding:4px;}"
    "QMenu::item{padding:6px 26px 6px 14px;border-radius:6px;}"
    "QMenu::item:selected{background-color:#243549;color:#38BDF8;}"
    "QMenu::separator{height:1px;background:#334155;margin:4px 8px;}"
    "QStatusBar{background-color:#0F172A;color:#94A3B8;border-top:1px solid #334155;}"
    "QCheckBox,QRadioButton{spacing:7px;color:#E2E8F0;background:transparent;}"
    "QCheckBox::indicator,QRadioButton::indicator{width:16px;height:16px;border:1px solid #475569;background:#0B1220;}"
    "QCheckBox::indicator{border-radius:4px;}"
    "QRadioButton::indicator{border-radius:9px;}"
    "QCheckBox::indicator:checked,QRadioButton::indicator:checked{background:#38BDF8;border-color:#38BDF8;}"
    "QCheckBox::indicator:hover,QRadioButton::indicator:hover{border-color:#38BDF8;}"
    "QSplitter::handle{background:#334155;}"
    "QSplitter::handle:vertical{height:6px;}"
    "QSplitter::handle:horizontal{width:6px;}"
    "QProgressBar{border:1px solid #334155;border-radius:8px;background:#0B1220;text-align:center;color:#E2E8F0;}"
    "QProgressBar::chunk{background-color:#38BDF8;border-radius:7px;}"
    "QLabel#lblStatus,QLabel#lblHeaderFieldStatus,QLabel#lblLiveFieldStatus,QLabel#lblInfo,QLabel#lblHeading,QLabel#lblLiveStatus{color:#BAE6FD;background-color:#0B2233;border:1px solid #1E3A5F;border-radius:8px;padding:8px 10px;}";
}

Themes::Mode Themes::currentMode()
{
    // Default is Light (the Modern Light look). Anyone who has toggled before
    // keeps their stored choice.
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

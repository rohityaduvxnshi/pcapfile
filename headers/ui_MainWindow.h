#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QAbstractItemView>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace Ui
{
class MainWindow
{
public:
    QWidget* centralWidget;

    QLineEdit* txtFilePath;
    QPushButton* btnBrowse;
    QPushButton* btnStart;

    QSpinBox* spinFilterCount;
    QRadioButton* radPortFilter;
    QRadioButton* radHeaderFilter;
    QWidget* portFilterPanel;
    QWidget* headerFilterPanel;
    QWidget* portFilterBoxContainer;
    QWidget* headerFilterBoxContainer;
    QVBoxLayout* portFilterBoxLayout;
    QVBoxLayout* headerFilterBoxLayout;
    QSpinBox* spinCommonPort;

    QPushButton* btnAddField;
    QPushButton* btnRemoveField;
    QPushButton* btnBitfieldDecoder;
    QTableWidget* tblFields;
    QTableWidget* tblOutput;
    QLabel* lblStatus;
    QProgressBar* progressBar;
    QMenuBar* menuBar;
    QStatusBar* statusBar;

    void setupUi(QMainWindow* window)
    {
        if (window->objectName().isEmpty())
        {
            window->setObjectName("MainWindow");
        }

        window->resize(1180, 820);
        window->setWindowTitle("PCAP UDP Extractor");

        centralWidget = new QWidget(window);
        QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

        QGroupBox* inputGroup = new QGroupBox("Input", centralWidget);
        QGridLayout* inputLayout = new QGridLayout(inputGroup);

        QLabel* lblFile = new QLabel("Capture File", inputGroup);
        txtFilePath = new QLineEdit(inputGroup);
        txtFilePath->setPlaceholderText("Select .pcap or .pcapng file");
        btnBrowse = new QPushButton("Browse", inputGroup);
        btnStart = new QPushButton("Start Export", inputGroup);
        btnStart->setMinimumWidth(130);

        inputLayout->addWidget(lblFile, 0, 0);
        inputLayout->addWidget(txtFilePath, 0, 1);
        inputLayout->addWidget(btnBrowse, 0, 2);
        inputLayout->addWidget(btnStart, 0, 3);

        QGroupBox* filterGroup = new QGroupBox("Message Filters", centralWidget);
        QVBoxLayout* filterMainLayout = new QVBoxLayout(filterGroup);

        QHBoxLayout* filterTopLayout = new QHBoxLayout();
        QLabel* lblFilterCount = new QLabel("Number of Message Filters", filterGroup);
        spinFilterCount = new QSpinBox(filterGroup);
        spinFilterCount->setRange(1, 20);
        spinFilterCount->setValue(1);
        radPortFilter = new QRadioButton("Port Filter", filterGroup);
        radHeaderFilter = new QRadioButton("Header Filter", filterGroup);
        radPortFilter->setChecked(true);

        filterTopLayout->addWidget(lblFilterCount);
        filterTopLayout->addWidget(spinFilterCount);
        filterTopLayout->addSpacing(20);
        filterTopLayout->addWidget(radPortFilter);
        filterTopLayout->addWidget(radHeaderFilter);
        filterTopLayout->addStretch();
        filterMainLayout->addLayout(filterTopLayout);

        portFilterPanel = new QWidget(filterGroup);
        QVBoxLayout* portPanelLayout = new QVBoxLayout(portFilterPanel);
        portPanelLayout->setContentsMargins(0, 0, 0, 0);
        QLabel* lblPortHelp = new QLabel("Enter one UDP port per filter. A packet matches a port if source OR destination UDP port equals that value.", portFilterPanel);
        lblPortHelp->setWordWrap(true);
        portFilterBoxContainer = new QWidget(portFilterPanel);
        portFilterBoxLayout = new QVBoxLayout(portFilterBoxContainer);
        portFilterBoxLayout->setContentsMargins(0, 0, 0, 0);
        portPanelLayout->addWidget(lblPortHelp);
        portPanelLayout->addWidget(portFilterBoxContainer);

        headerFilterPanel = new QWidget(filterGroup);
        QVBoxLayout* headerPanelLayout = new QVBoxLayout(headerFilterPanel);
        headerPanelLayout->setContentsMargins(0, 0, 0, 0);

        QHBoxLayout* commonPortLayout = new QHBoxLayout();
        QLabel* lblCommonPort = new QLabel("Common UDP Port", headerFilterPanel);
        spinCommonPort = new QSpinBox(headerFilterPanel);
        spinCommonPort->setRange(0, 65535);
        spinCommonPort->setValue(5000);
        commonPortLayout->addWidget(lblCommonPort);
        commonPortLayout->addWidget(spinCommonPort);
        commonPortLayout->addStretch();

        QLabel* lblHeaderHelp = new QLabel("Enter header prefixes in hex. Allowed length: 0 to 4 bytes (0, 2, 4, 6, or 8 hex characters). Matching is case-insensitive and starts at byte 0 of the UDP payload.", headerFilterPanel);
        lblHeaderHelp->setWordWrap(true);
        headerFilterBoxContainer = new QWidget(headerFilterPanel);
        headerFilterBoxLayout = new QVBoxLayout(headerFilterBoxContainer);
        headerFilterBoxLayout->setContentsMargins(0, 0, 0, 0);

        headerPanelLayout->addLayout(commonPortLayout);
        headerPanelLayout->addWidget(lblHeaderHelp);
        headerPanelLayout->addWidget(headerFilterBoxContainer);

        filterMainLayout->addWidget(portFilterPanel);
        filterMainLayout->addWidget(headerFilterPanel);

        QGroupBox* fieldGroup = new QGroupBox("UDP Payload Field Definitions", centralWidget);
        QVBoxLayout* fieldLayout = new QVBoxLayout(fieldGroup);
        QHBoxLayout* fieldButtonLayout = new QHBoxLayout();
        btnAddField = new QPushButton("Add Field", fieldGroup);
        btnRemoveField = new QPushButton("Remove Selected Field", fieldGroup);
        btnBitfieldDecoder = new QPushButton("Bitfield Decoder", fieldGroup);
        fieldButtonLayout->addWidget(btnAddField);
        fieldButtonLayout->addWidget(btnRemoveField);
        fieldButtonLayout->addWidget(btnBitfieldDecoder);
        fieldButtonLayout->addStretch();

        tblFields = new QTableWidget(fieldGroup);
        tblFields->setMinimumHeight(150);
        fieldLayout->addLayout(fieldButtonLayout);
        fieldLayout->addWidget(tblFields);

        QGroupBox* outputGroup = new QGroupBox("Output Preview", centralWidget);
        QVBoxLayout* outputLayout = new QVBoxLayout(outputGroup);
        tblOutput = new QTableWidget(outputGroup);
        tblOutput->setMinimumHeight(300);
        outputLayout->addWidget(tblOutput);

        QHBoxLayout* statusLayout = new QHBoxLayout();
        lblStatus = new QLabel("Ready", centralWidget);
        progressBar = new QProgressBar(centralWidget);
        progressBar->setRange(0, 100);
        progressBar->setValue(0);
        statusLayout->addWidget(lblStatus, 1);
        statusLayout->addWidget(progressBar);

        mainLayout->addWidget(inputGroup);
        mainLayout->addWidget(filterGroup);
        mainLayout->addWidget(fieldGroup);
        mainLayout->addWidget(outputGroup, 1);
        mainLayout->addLayout(statusLayout);

        window->setCentralWidget(centralWidget);
        menuBar = new QMenuBar(window);
        window->setMenuBar(menuBar);
        statusBar = new QStatusBar(window);
        window->setStatusBar(statusBar);
    }
};
}

#endif // UI_MAINWINDOW_H

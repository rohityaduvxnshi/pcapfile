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
    QSpinBox* spinPort;
    QPushButton* btnStart;
    QPushButton* btnAddField;
    QPushButton* btnRemoveField;
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

        window->resize(1100, 760);
        window->setWindowTitle("PCAP UDP Extractor");

        centralWidget = new QWidget(window);
        QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

        QGroupBox* inputGroup = new QGroupBox("Input", centralWidget);
        QGridLayout* inputLayout = new QGridLayout(inputGroup);

        QLabel* lblFile = new QLabel("Capture File", inputGroup);
        txtFilePath = new QLineEdit(inputGroup);
        txtFilePath->setPlaceholderText("Select .pcap or .pcapng file");
        btnBrowse = new QPushButton("Browse", inputGroup);

        QLabel* lblPort = new QLabel("UDP Port", inputGroup);
        spinPort = new QSpinBox(inputGroup);
        spinPort->setRange(0, 65535);
        spinPort->setValue(5000);
        btnStart = new QPushButton("Start", inputGroup);
        btnStart->setMinimumWidth(120);

        inputLayout->addWidget(lblFile, 0, 0);
        inputLayout->addWidget(txtFilePath, 0, 1);
        inputLayout->addWidget(btnBrowse, 0, 2);
        inputLayout->addWidget(lblPort, 1, 0);
        inputLayout->addWidget(spinPort, 1, 1);
        inputLayout->addWidget(btnStart, 1, 2);

        QGroupBox* fieldGroup = new QGroupBox("UDP Payload Field Definitions", centralWidget);
        QVBoxLayout* fieldLayout = new QVBoxLayout(fieldGroup);
        QHBoxLayout* fieldButtonLayout = new QHBoxLayout();
        btnAddField = new QPushButton("Add Field", fieldGroup);
        btnRemoveField = new QPushButton("Remove Selected Field", fieldGroup);
        fieldButtonLayout->addWidget(btnAddField);
        fieldButtonLayout->addWidget(btnRemoveField);
        fieldButtonLayout->addStretch();

        tblFields = new QTableWidget(fieldGroup);
        tblFields->setMinimumHeight(150);
        fieldLayout->addLayout(fieldButtonLayout);
        fieldLayout->addWidget(tblFields);

        QGroupBox* outputGroup = new QGroupBox("Output Preview", centralWidget);
        QVBoxLayout* outputLayout = new QVBoxLayout(outputGroup);
        tblOutput = new QTableWidget(outputGroup);
        tblOutput->setMinimumHeight(320);
        outputLayout->addWidget(tblOutput);

        QHBoxLayout* statusLayout = new QHBoxLayout();
        lblStatus = new QLabel("Ready", centralWidget);
        progressBar = new QProgressBar(centralWidget);
        progressBar->setRange(0, 100);
        progressBar->setValue(0);
        statusLayout->addWidget(lblStatus, 1);
        statusLayout->addWidget(progressBar);

        mainLayout->addWidget(inputGroup);
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

#endif

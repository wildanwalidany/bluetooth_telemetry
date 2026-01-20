#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QWidget>
#include <QDateTime>
#include <QFont>
#include <QScrollBar>
#include <QMessageBox>
#include <QPalette>

#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), 
      serverSocket(-1), 
      clientSocket(-1),
      serverNotifier(nullptr),
      clientNotifier(nullptr),
      isRunning(false),
      echoMode(false),
      hexMode(false),
      msgCount(0),
      totalBytes(0),
      blinkAnimation(nullptr)
{
    setWindowTitle("Bluetooth Telemetry Server - Modern UI");
    setGeometry(100, 100, 1200, 900);
    
    applyModernStyle();
    setupUI();
    
    // Timer to check client connection status
    clientCheckTimer = new QTimer(this);
    connect(clientCheckTimer, &QTimer::timeout, this, &MainWindow::checkClientConnection);
}

MainWindow::~MainWindow()
{
    stopBluetoothServer();
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // Control panel with modern styling
    QGroupBox *controlGroup = new QGroupBox("Server Control", this);
    controlGroup->setStyleSheet(
        "QGroupBox { "
        "  font-weight: bold; "
        "  font-size: 14px; "
        "  border: 2px solid #3498db; "
        "  border-radius: 8px; "
        "  margin-top: 10px; "
        "  padding-top: 15px; "
        "} "
        "QGroupBox::title { "
        "  subcontrol-origin: margin; "
        "  subcontrol-position: top left; "
        "  padding: 0 10px; "
        "  color: #3498db; "
        "}"
    );
    QHBoxLayout *controlLayout = new QHBoxLayout(controlGroup);
    controlLayout->setSpacing(15);
    
    startButton = new QPushButton("▶ Start Server", this);
    startButton->setMinimumHeight(40);
    startButton->setStyleSheet(
        "QPushButton { "
        "  background-color: #27ae60; "
        "  color: white; "
        "  font-size: 14px; "
        "  font-weight: bold; "
        "  border: none; "
        "  border-radius: 6px; "
        "  padding: 10px 20px; "
        "} "
        "QPushButton:hover { "
        "  background-color: #2ecc71; "
        "} "
        "QPushButton:pressed { "
        "  background-color: #229954; "
        "} "
        "QPushButton:disabled { "
        "  background-color: #95a5a6; "
        "}"
    );
    
    stopButton = new QPushButton("⏹ Stop Server", this);
    stopButton->setEnabled(false);
    stopButton->setMinimumHeight(40);
    stopButton->setStyleSheet(
        "QPushButton { "
        "  background-color: #e74c3c; "
        "  color: white; "
        "  font-size: 14px; "
        "  font-weight: bold; "
        "  border: none; "
        "  border-radius: 6px; "
        "  padding: 10px 20px; "
        "} "
        "QPushButton:hover { "
        "  background-color: #ec7063; "
        "} "
        "QPushButton:pressed { "
        "  background-color: #c0392b; "
        "} "
        "QPushButton:disabled { "
        "  background-color: #95a5a6; "
        "}"
    );
    
    echoModeCheck = new QCheckBox("Echo Mode", this);
    echoModeCheck->setStyleSheet("QCheckBox { font-size: 13px; padding: 5px; }");
    
    hexModeCheck = new QCheckBox("Hex Output", this);
    hexModeCheck->setChecked(true);
    hexModeCheck->setStyleSheet("QCheckBox { font-size: 13px; padding: 5px; }");
    
    controlLayout->addWidget(startButton);
    controlLayout->addWidget(stopButton);
    controlLayout->addWidget(echoModeCheck);
    controlLayout->addWidget(hexModeCheck);
    controlLayout->addStretch();
    
    connect(startButton, &QPushButton::clicked, this, &MainWindow::onStartServer);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::onStopServer);
    
    mainLayout->addWidget(controlGroup);
    
    // Status with visual indicator
    QGroupBox *statusGroup = new QGroupBox("Connection Status", this);
    statusGroup->setStyleSheet(
        "QGroupBox { "
        "  font-weight: bold; "
        "  font-size: 14px; "
        "  border: 2px solid #9b59b6; "
        "  border-radius: 8px; "
        "  margin-top: 10px; "
        "  padding-top: 15px; "
        "} "
        "QGroupBox::title { "
        "  subcontrol-origin: margin; "
        "  subcontrol-position: top left; "
        "  padding: 0 10px; "
        "  color: #9b59b6; "
        "}"
    );
    QHBoxLayout *statusLayout = new QHBoxLayout(statusGroup);
    statusLayout->setSpacing(15);
    
    statusIndicator = new QLabel("●", this);
    statusIndicator->setStyleSheet(
        "QLabel { "
        "  color: #95a5a6; "
        "  font-size: 32px; "
        "  padding: 0 10px; "
        "}"
    );
    
    QVBoxLayout *statusTextLayout = new QVBoxLayout();
    statusLabel = new QLabel("Server: Stopped", this);
    statusLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #2c3e50; }");
    
    clientAddressLabel = new QLabel("Client: None", this);
    clientAddressLabel->setStyleSheet("QLabel { font-size: 13px; color: #34495e; }");
    
    statusTextLayout->addWidget(statusLabel);
    statusTextLayout->addWidget(clientAddressLabel);
    
    statusLayout->addWidget(statusIndicator);
    statusLayout->addLayout(statusTextLayout);
    statusLayout->addStretch();
    
    mainLayout->addWidget(statusGroup);
    
    // Telemetry display
    QGroupBox *telemetryGroup = nullptr;
    createTelemetryGroup(telemetryGroup);
    mainLayout->addWidget(telemetryGroup);
    
    // Statistics
    QGroupBox *statsGroup = nullptr;
    createStatsGroup(statsGroup);
    mainLayout->addWidget(statsGroup);
    
    // Log output
    QGroupBox *logGroup = new QGroupBox("Server Log", this);
    logGroup->setStyleSheet(
        "QGroupBox { "
        "  font-weight: bold; "
        "  font-size: 14px; "
        "  border: 2px solid #e67e22; "
        "  border-radius: 8px; "
        "  margin-top: 10px; "
        "  padding-top: 15px; "
        "} "
        "QGroupBox::title { "
        "  subcontrol-origin: margin; "
        "  subcontrol-position: top left; "
        "  padding: 0 10px; "
        "  color: #e67e22; "
        "}"
    );
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    logOutput = new QTextEdit(this);
    logOutput->setReadOnly(true);
    logOutput->setMaximumHeight(200);
    logOutput->setStyleSheet(
        "QTextEdit { "
        "  background-color: #2c3e50; "
        "  color: #ecf0f1; "
        "  font-family: 'Courier New', monospace; "
        "  font-size: 12px; "
        "  border: 1px solid #34495e; "
        "  border-radius: 4px; "
        "  padding: 10px; "
        "}"
    );
    logLayout->addWidget(logOutput);
    mainLayout->addWidget(logGroup);
}

void MainWindow::createTelemetryGroup(QGroupBox *&group)
{
    group = new QGroupBox("Real-Time Telemetry", this);
    group->setStyleSheet(
        "QGroupBox { "
        "  font-weight: bold; "
        "  font-size: 14px; "
        "  border: 2px solid #16a085; "
        "  border-radius: 8px; "
        "  margin-top: 10px; "
        "  padding-top: 15px; "
        "} "
        "QGroupBox::title { "
        "  subcontrol-origin: margin; "
        "  subcontrol-position: top left; "
        "  padding: 0 10px; "
        "  color: #16a085; "
        "}"
    );
    
    QGridLayout *layout = new QGridLayout(group);
    layout->setSpacing(12);
    layout->setContentsMargins(15, 20, 15, 15);
    
    int row = 0;
    
    // Speed - Large display
    QLabel *speedTitle = new QLabel("⚡ Speed:", this);
    speedTitle->setStyleSheet("QLabel { font-size: 13px; font-weight: bold; color: #2c3e50; }");
    layout->addWidget(speedTitle, row, 0);
    speedLabel = new QLabel("-- RPM", this);
    speedLabel->setStyleSheet(
        "QLabel { "
        "  font-size: 24px; "
        "  font-weight: bold; "
        "  color: #3498db; "
        "  background-color: #ecf0f1; "
        "  border-radius: 6px; "
        "  padding: 10px 20px; "
        "}"
    );
    layout->addWidget(speedLabel, row++, 1, 1, 3);
    
    // Throttle
    layout->addWidget(new QLabel("⏱ Throttle:", this), row, 0);
    throttleLabel = new QLabel("--", this);
    throttleLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #34495e; padding: 5px; }");
    layout->addWidget(throttleLabel, row++, 1);
    
    // Odometer
    layout->addWidget(new QLabel("👋 Odometer:", this), row, 0);
    odometerLabel = new QLabel("-- km", this);
    odometerLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #34495e; padding: 5px; }");
    layout->addWidget(odometerLabel, row++, 1);
    
    // Battery with progress bar
    layout->addWidget(new QLabel("🔋 Battery:", this), row, 0);
    QHBoxLayout *batteryLayout = new QHBoxLayout();
    batteryLabel = new QLabel("--%", this);
    batteryLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #27ae60; padding: 5px; min-width: 50px; }");
    batteryBar = new QProgressBar(this);
    batteryBar->setRange(0, 100);
    batteryBar->setTextVisible(false);
    batteryBar->setMaximumHeight(20);
    batteryBar->setStyleSheet(
        "QProgressBar { "
        "  border: 2px solid #bdc3c7; "
        "  border-radius: 5px; "
        "  background-color: #ecf0f1; "
        "} "
        "QProgressBar::chunk { "
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #27ae60, stop:1 #2ecc71); "
        "  border-radius: 3px; "
        "}"
    );
    batteryLayout->addWidget(batteryLabel);
    batteryLayout->addWidget(batteryBar);
    layout->addLayout(batteryLayout, row++, 1, 1, 3);
    
    // Engine Temp with progress bar and color coding
    layout->addWidget(new QLabel("🌡️ Engine Temp:", this), row, 0);
    QHBoxLayout *tempLayout = new QHBoxLayout();
    engineTempLabel = new QLabel("-- °C", this);
    engineTempLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; padding: 5px; min-width: 60px; }");
    engineTempBar = new QProgressBar(this);
    engineTempBar->setRange(-20, 43);
    engineTempBar->setTextVisible(false);
    engineTempBar->setMaximumHeight(20);
    engineTempBar->setStyleSheet(
        "QProgressBar { "
        "  border: 2px solid #bdc3c7; "
        "  border-radius: 5px; "
        "  background-color: #ecf0f1; "
        "} "
        "QProgressBar::chunk { "
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3498db, stop:0.5 #f39c12, stop:1 #e74c3c); "
        "  border-radius: 3px; "
        "}"
    );
    tempLayout->addWidget(engineTempLabel);
    tempLayout->addWidget(engineTempBar);
    layout->addLayout(tempLayout, row++, 1, 1, 3);
    
    // Battery Temp
    layout->addWidget(new QLabel("🌡️ Battery Temp:", this), row, 0);
    batteryTempLabel = new QLabel("--", this);
    batteryTempLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #34495e; padding: 5px; }");
    layout->addWidget(batteryTempLabel, row++, 1);
    
    // Second column - State indicators
    row = 0;
    
    // State with styled frame
    layout->addWidget(new QLabel("⚙️ State:", this), row, 4);
    QHBoxLayout *stateLayout = new QHBoxLayout();
    stateFrame = createIndicatorFrame();
    stateLabel = new QLabel("--", this);
    stateLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; padding: 5px; }");
    stateLayout->addWidget(stateFrame);
    stateLayout->addWidget(stateLabel);
    stateLayout->addStretch();
    layout->addLayout(stateLayout, row++, 5);
    
    // Mode with styled frame
    layout->addWidget(new QLabel("🏎️ Mode:", this), row, 4);
    QHBoxLayout *modeLayout = new QHBoxLayout();
    modeFrame = createIndicatorFrame();
    modeLabel = new QLabel("--", this);
    modeLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; padding: 5px; }");
    modeLayout->addWidget(modeFrame);
    modeLayout->addWidget(modeLabel);
    modeLayout->addStretch();
    layout->addLayout(modeLayout, row++, 5);
    
    // Turn Signal with indicator
    layout->addWidget(new QLabel("➡️ Turn Signal:", this), row, 4);
    QHBoxLayout *turnLayout = new QHBoxLayout();
    turnSignalFrame = createIndicatorFrame();
    turnSignalLabel = new QLabel("--", this);
    turnSignalLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; padding: 5px; }");
    turnLayout->addWidget(turnSignalFrame);
    turnLayout->addWidget(turnSignalLabel);
    turnLayout->addStretch();
    layout->addLayout(turnLayout, row++, 5);
    
    // Night Mode with indicator
    layout->addWidget(new QLabel("🌙 Night Mode:", this), row, 4);
    QHBoxLayout *nightLayout = new QHBoxLayout();
    nightModeFrame = createIndicatorFrame();
    nightModeLabel = new QLabel("OFF", this);
    nightModeLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; padding: 5px; }");
    nightLayout->addWidget(nightModeFrame);
    nightLayout->addWidget(nightModeLabel);
    nightLayout->addStretch();
    layout->addLayout(nightLayout, row++, 5);
    
    // High Beam with indicator
    layout->addWidget(new QLabel("💡 High Beam:", this), row, 4);
    QHBoxLayout *beamLayout = new QHBoxLayout();
    beamFrame = createIndicatorFrame();
    beamLabel = new QLabel("OFF", this);
    beamLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; padding: 5px; }");
    beamLayout->addWidget(beamFrame);
    beamLayout->addWidget(beamLabel);
    beamLayout->addStretch();
    layout->addLayout(beamLayout, row++, 5);
    
    // Horn with indicator
    layout->addWidget(new QLabel("📢 Horn:", this), row, 4);
    QHBoxLayout *hornLayout = new QHBoxLayout();
    hornFrame = createIndicatorFrame();
    hornLabel = new QLabel("OFF", this);
    hornLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; padding: 5px; }");
    hornLayout->addWidget(hornFrame);
    hornLayout->addWidget(hornLabel);
    hornLayout->addStretch();
    layout->addLayout(hornLayout, row++, 5);
    
    // Alert
    layout->addWidget(new QLabel("⚠️ Alert:", this), row, 4);
    alertLabel = new QLabel("--", this);
    alertLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #e74c3c; padding: 5px; }");
    layout->addWidget(alertLabel, row++, 5);
    
    // Maps
    layout->addWidget(new QLabel("🗺️ Maps:", this), row, 4);
    mapsLabel = new QLabel("--", this);
    mapsLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #34495e; padding: 5px; }");
    layout->addWidget(mapsLabel, row++, 5);
}

void MainWindow::createStatsGroup(QGroupBox *&group)
{
    group = new QGroupBox("Statistics", this);
    group->setStyleSheet(
        "QGroupBox { "
        "  font-weight: bold; "
        "  font-size: 14px; "
        "  border: 2px solid #f39c12; "
        "  border-radius: 8px; "
        "  margin-top: 10px; "
        "  padding-top: 15px; "
        "} "
        "QGroupBox::title { "
        "  subcontrol-origin: margin; "
        "  subcontrol-position: top left; "
        "  padding: 0 10px; "
        "  color: #f39c12; "
        "}"
    );
    
    QHBoxLayout *layout = new QHBoxLayout(group);
    layout->setSpacing(20);
    layout->setContentsMargins(15, 20, 15, 15);
    
    QLabel *msgTitle = new QLabel("📨 Messages:", this);
    msgTitle->setStyleSheet("QLabel { font-size: 13px; font-weight: bold; color: #2c3e50; }");
    layout->addWidget(msgTitle);
    
    msgCountLabel = new QLabel("0", this);
    msgCountLabel->setStyleSheet(
        "QLabel { "
        "  font-size: 18px; "
        "  font-weight: bold; "
        "  color: #3498db; "
        "  background-color: #ecf0f1; "
        "  border-radius: 5px; "
        "  padding: 8px 15px; "
        "  min-width: 80px; "
        "}"
    );
    layout->addWidget(msgCountLabel);
    
    layout->addSpacing(30);
    
    QLabel *bytesTitle = new QLabel("📊 Total Bytes:", this);
    bytesTitle->setStyleSheet("QLabel { font-size: 13px; font-weight: bold; color: #2c3e50; }");
    layout->addWidget(bytesTitle);
    
    totalBytesLabel = new QLabel("0", this);
    totalBytesLabel->setStyleSheet(
        "QLabel { "
        "  font-size: 18px; "
        "  font-weight: bold; "
        "  color: #27ae60; "
        "  background-color: #ecf0f1; "
        "  border-radius: 5px; "
        "  padding: 8px 15px; "
        "  min-width: 100px; "
        "}"
    );
    layout->addWidget(totalBytesLabel);
    
    layout->addStretch();
}

void MainWindow::onStartServer()
{
    echoMode = echoModeCheck->isChecked();
    hexMode = hexModeCheck->isChecked();
    
    if (startBluetoothServer()) {
        isRunning = true;
        startButton->setEnabled(false);
        stopButton->setEnabled(true);
        echoModeCheck->setEnabled(false);
        hexModeCheck->setEnabled(false);
        
        updateStatusIndicator(true, false);
        statusLabel->setText("Server: Running on channel 1");
        statusLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #27ae60; }");
        logMessage("[INFO] RFCOMM server started successfully");
        logMessage(QString("[INFO] Echo mode: %1, Hex output: %2")
                   .arg(echoMode ? "ON" : "OFF")
                   .arg(hexMode ? "ON" : "OFF"));
    } else {
        QMessageBox::critical(this, "Error", 
                            "Failed to start Bluetooth server. Make sure you run with sudo privileges.");
    }
}

void MainWindow::onStopServer()
{
    stopBluetoothServer();
    isRunning = false;
    startButton->setEnabled(true);
    stopButton->setEnabled(false);
    echoModeCheck->setEnabled(true);
    hexModeCheck->setEnabled(true);
    
    updateStatusIndicator(false, false);
    statusLabel->setText("Server: Stopped");
    statusLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #2c3e50; }");
    clientAddressLabel->setText("Client: None");
    logMessage("[INFO] Server stopped");
}

bool MainWindow::startBluetoothServer()
{
    struct sockaddr_rc loc_addr = {0};
    
    serverSocket = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (serverSocket < 0) {
        logMessage(QString("[ERROR] Failed to create socket: %1").arg(strerror(errno)));
        return false;
    }
    
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = (bdaddr_t){{0, 0, 0, 0, 0, 0}};
    loc_addr.rc_channel = 1;
    
    if (bind(serverSocket, (struct sockaddr *)&loc_addr, sizeof(loc_addr)) < 0) {
        logMessage(QString("[ERROR] Failed to bind: %1").arg(strerror(errno)));
        ::close(serverSocket);
        serverSocket = -1;
        return false;
    }
    
    if (listen(serverSocket, 1) < 0) {
        logMessage(QString("[ERROR] Failed to listen: %1").arg(strerror(errno)));
        ::close(serverSocket);
        serverSocket = -1;
        return false;
    }
    
    // Set up socket notifier for incoming connections
    serverNotifier = new QSocketNotifier(serverSocket, QSocketNotifier::Read, this);
    connect(serverNotifier, &QSocketNotifier::activated, this, &MainWindow::onServerSocketReady);
    
    logMessage("[INFO] Waiting for client connections...");
    
    return true;
}

void MainWindow::stopBluetoothServer()
{
    if (clientNotifier) {
        clientNotifier->setEnabled(false);
        delete clientNotifier;
        clientNotifier = nullptr;
    }
    
    if (clientSocket >= 0) {
        ::close(clientSocket);
        clientSocket = -1;
    }
    
    if (serverNotifier) {
        serverNotifier->setEnabled(false);
        delete serverNotifier;
        serverNotifier = nullptr;
    }
    
    if (serverSocket >= 0) {
        ::close(serverSocket);
        serverSocket = -1;
    }
    
    if (clientCheckTimer) {
        clientCheckTimer->stop();
    }
}

void MainWindow::onServerSocketReady()
{
    acceptClientConnection();
}

void MainWindow::acceptClientConnection()
{
    struct sockaddr_rc rem_addr = {0};
    socklen_t opt = sizeof(rem_addr);
    char addr[18] = {0};
    
    clientSocket = accept(serverSocket, (struct sockaddr *)&rem_addr, &opt);
    if (clientSocket < 0) {
        logMessage(QString("[ERROR] Accept failed: %1").arg(strerror(errno)));
        return;
    }
    
    ba2str(&rem_addr.rc_bdaddr, addr);
    clientAddressLabel->setText(QString("Client: %1").arg(addr));
    clientAddressLabel->setStyleSheet("QLabel { font-size: 13px; color: #27ae60; font-weight: bold; }");
    updateStatusIndicator(true, true);
    logMessage(QString("%1 [INFO] Client connected: %2").arg(getTimestamp()).arg(addr));
    
    msgCount = 0;
    totalBytes = 0;
    msgCountLabel->setText("0");
    totalBytesLabel->setText("0");
    
    // Set up notifier for client data
    clientNotifier = new QSocketNotifier(clientSocket, QSocketNotifier::Read, this);
    connect(clientNotifier, &QSocketNotifier::activated, this, &MainWindow::onClientSocketReady);
    
    // Start timer to check connection
    clientCheckTimer->start(1000);
}

void MainWindow::onClientSocketReady()
{
    handleClientData();
}

void MainWindow::handleClientData()
{
    uint8_t buf[1024];
    ssize_t bytes_read;
    
    bytes_read = recv(clientSocket, buf, sizeof(buf) - 1, 0);
    
    if (bytes_read < 0) {
        if (errno != EINTR && errno != EAGAIN) {
            logMessage(QString("[ERROR] recv failed: %1").arg(strerror(errno)));
            // Close client connection
            if (clientNotifier) {
                clientNotifier->setEnabled(false);
                delete clientNotifier;
                clientNotifier = nullptr;
            }
            ::close(clientSocket);
            clientSocket = -1;
            clientAddressLabel->setText("Client: Disconnected");
            clientCheckTimer->stop();
        }
        return;
    }
    
    if (bytes_read == 0) {
        logMessage(QString("%1 [INFO] Client disconnected").arg(getTimestamp()));
        if (clientNotifier) {
            clientNotifier->setEnabled(false);
            delete clientNotifier;
            clientNotifier = nullptr;
        }
        ::close(clientSocket);
        clientSocket = -1;
        clientAddressLabel->setText("Client: Disconnected");
        clientAddressLabel->setStyleSheet("QLabel { font-size: 13px; color: #e74c3c; font-weight: bold; }");
        updateStatusIndicator(true, false);
        clientCheckTimer->stop();
        return;
    }
    
    totalBytes += bytes_read;
    msgCount++;
    
    msgCountLabel->setText(QString::number(msgCount));
    totalBytesLabel->setText(QString::number(totalBytes));
    
    logMessage(QString("%1 [RX] %2 bytes").arg(getTimestamp()).arg(bytes_read));
    
    if (hexMode) {
        logHex(buf, bytes_read);
    }
    
    // Parse telemetry
    telemetry_t telem;
    int parse_result = parseTelemetry(buf, bytes_read, &telem);
    if (parse_result == 0) {
        displayTelemetry(&telem);
    } else {
        logMessage(QString("[WARN] Failed to parse telemetry (code: %1)").arg(parse_result));
    }
    
    // Echo if enabled
    if (echoMode) {
        ssize_t sent = send(clientSocket, buf, bytes_read, 0);
        if (sent < 0) {
            logMessage(QString("[ERROR] send failed: %1").arg(strerror(errno)));
        } else {
            logMessage(QString("%1 [TX] Echoed %2 bytes").arg(getTimestamp()).arg(sent));
        }
    }
}

void MainWindow::checkClientConnection()
{
    if (clientSocket < 0) {
        clientCheckTimer->stop();
    }
}

int MainWindow::parseTelemetry(const uint8_t *data, size_t len, telemetry_t *telem)
{
    // Expected frame: 0xCE, len(8), data[8], '\n' = 11 bytes
    if (len < 11) {
        return -1;  // incomplete frame
    }
    
    if (data[0] != 0xCE) {
        return -2;  // invalid start marker
    }
    
    if (data[1] != 8) {
        return -3;  // invalid length
    }
    
    if (data[10] != '\n') {
        return -4;  // missing delimiter
    }
    
    // Parse payload
    telem->speed = data[2];
    telem->throttle = data[3];
    telem->total_miles = (uint16_t)data[4] | ((uint16_t)data[5] << 8);
    telem->battery = data[6] & 0x7F;
    telem->night_mode = (data[6] >> 7) & 0x01;
    telem->engine_temp = data[7] & 0x3F;
    telem->turn_signal = (data[7] >> 6) & 0x03;
    telem->battery_temp = data[8] & 0x3F;
    telem->horn = (data[8] >> 6) & 0x01;
    telem->beam = (data[8] >> 7) & 0x01;
    telem->alert = data[9] & 0x07;
    telem->state = (data[9] >> 3) & 0x03;
    telem->mode = (data[9] >> 5) & 0x03;
    telem->maps = (data[9] >> 7) & 0x01;
    
    return 0;
}

void MainWindow::displayTelemetry(const telemetry_t *telem)
{
    const char *state_str[] = {"", "N", "D", "P"};
    const char *mode_str[] = {"", "ECON", "COMF", "SPORT"};
    const char *signal_str[] = {"none", "right", "left", "hazard"};
    
    // Speed - prominent display
    int rpm = telem->speed * 46;
    speedLabel->setText(QString("%1 RPM").arg(rpm));
    if (rpm > 8000) {
        speedLabel->setStyleSheet(
            "QLabel { font-size: 24px; font-weight: bold; color: #e74c3c; "
            "background-color: #fadbd8; border-radius: 6px; padding: 10px 20px; }");
    } else if (rpm > 5000) {
        speedLabel->setStyleSheet(
            "QLabel { font-size: 24px; font-weight: bold; color: #f39c12; "
            "background-color: #fef5e7; border-radius: 6px; padding: 10px 20px; }");
    } else {
        speedLabel->setStyleSheet(
            "QLabel { font-size: 24px; font-weight: bold; color: #3498db; "
            "background-color: #ecf0f1; border-radius: 6px; padding: 10px 20px; }");
    }
    
    // Throttle
    throttleLabel->setText(QString::number(telem->throttle));
    
    // Odometer
    odometerLabel->setText(QString("%1 km").arg(telem->total_miles * 1.60934, 0, 'f', 1));
    
    // Battery with progress bar and color coding
    batteryLabel->setText(QString("%1%").arg(telem->battery));
    batteryBar->setValue(telem->battery);
    if (telem->battery < 20) {
        batteryLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #e74c3c; padding: 5px; min-width: 50px; }");
        batteryBar->setStyleSheet(
            "QProgressBar { border: 2px solid #e74c3c; border-radius: 5px; background-color: #fadbd8; } "
            "QProgressBar::chunk { background-color: #e74c3c; border-radius: 3px; }");
    } else if (telem->battery < 50) {
        batteryLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #f39c12; padding: 5px; min-width: 50px; }");
        batteryBar->setStyleSheet(
            "QProgressBar { border: 2px solid #f39c12; border-radius: 5px; background-color: #fef5e7; } "
            "QProgressBar::chunk { background-color: #f39c12; border-radius: 3px; }");
    } else {
        batteryLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #27ae60; padding: 5px; min-width: 50px; }");
        batteryBar->setStyleSheet(
            "QProgressBar { border: 2px solid #bdc3c7; border-radius: 5px; background-color: #ecf0f1; } "
            "QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #27ae60, stop:1 #2ecc71); border-radius: 3px; }");
    }
    
    // Engine Temperature with bar and color coding
    int engineTemp = (int)telem->engine_temp - 20;
    engineTempLabel->setText(QString("%1 °C").arg(engineTemp));
    engineTempBar->setValue(engineTemp);
    if (engineTemp > 90) {
        engineTempLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #e74c3c; padding: 5px; min-width: 60px; }");
    } else if (engineTemp > 70) {
        engineTempLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #f39c12; padding: 5px; min-width: 60px; }");
    } else {
        engineTempLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #3498db; padding: 5px; min-width: 60px; }");
    }
    
    // Battery Temp
    batteryTempLabel->setText(QString::number(telem->battery_temp));
    
    // State with indicator
    updateIndicatorState(stateFrame, stateLabel, telem->state > 0, state_str[telem->state]);
    
    // Mode with color coding
    modeLabel->setText(mode_str[telem->mode]);
    QString modeColor;
    switch(telem->mode) {
        case 1: modeColor = "#27ae60"; break; // ECON - green
        case 2: modeColor = "#3498db"; break; // COMF - blue
        case 3: modeColor = "#e74c3c"; break; // SPORT - red
        default: modeColor = "#95a5a6"; break;
    }
    modeFrame->setStyleSheet(QString(
        "QFrame { background-color: %1; border-radius: 8px; min-width: 16px; max-width: 16px; "
        "min-height: 16px; max-height: 16px; }").arg(modeColor));
    
    // Turn Signal with animated indicator
    turnSignalLabel->setText(signal_str[telem->turn_signal]);
    bool turnActive = (telem->turn_signal > 0);
    if (turnActive) {
        QString turnColor = (telem->turn_signal == 3) ? "#f39c12" : "#2ecc71"; // hazard=orange, else=green
        turnSignalFrame->setStyleSheet(QString(
            "QFrame { background-color: %1; border-radius: 8px; min-width: 16px; max-width: 16px; "
            "min-height: 16px; max-height: 16px; }").arg(turnColor));
    } else {
        turnSignalFrame->setStyleSheet(
            "QFrame { background-color: #95a5a6; border-radius: 8px; min-width: 16px; max-width: 16px; "
            "min-height: 16px; max-height: 16px; }");
    }
    
    // Night Mode
    updateIndicatorState(nightModeFrame, nightModeLabel, telem->night_mode, telem->night_mode ? "ON" : "OFF");
    
    // High Beam
    updateIndicatorState(beamFrame, beamLabel, telem->beam, telem->beam ? "ON" : "OFF");
    
    // Horn
    updateIndicatorState(hornFrame, hornLabel, telem->horn, telem->horn ? "ON" : "OFF");
    
    // Alert
    alertLabel->setText(QString::number(telem->alert));
    if (telem->alert > 0) {
        alertLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #e74c3c; "
                                   "background-color: #fadbd8; padding: 5px 10px; border-radius: 4px; }");
    } else {
        alertLabel->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #27ae60; padding: 5px; }");
    }
    
    // Maps
    mapsLabel->setText(telem->maps ? "ON" : "OFF");
}

void MainWindow::logMessage(const QString &msg)
{
    logOutput->append(msg);
    // Auto-scroll to bottom
    QScrollBar *scrollBar = logOutput->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void MainWindow::logHex(const uint8_t *data, size_t len)
{
    QString hexStr = "[HEX] ";
    for (size_t i = 0; i < len; i++) {
        hexStr += QString("%1 ").arg(data[i], 2, 16, QChar('0')).toUpper();
    }
    logMessage(hexStr);
}

QString MainWindow::getTimestamp()
{
    return QDateTime::currentDateTime().toString("[HH:mm:ss]");
}

void MainWindow::applyModernStyle()
{
    // Set modern application-wide style
    setStyleSheet(
        "QMainWindow { background-color: #f5f6fa; }"
        "QWidget { font-family: 'Segoe UI', Arial, sans-serif; }"
        "QLabel { color: #2c3e50; }"
        "QGroupBox { background-color: white; }"
    );
}

QFrame* MainWindow::createIndicatorFrame()
{
    QFrame *frame = new QFrame(this);
    frame->setFrameShape(QFrame::NoFrame);
    frame->setStyleSheet(
        "QFrame { "
        "  background-color: #95a5a6; "
        "  border-radius: 8px; "
        "  min-width: 16px; "
        "  max-width: 16px; "
        "  min-height: 16px; "
        "  max-height: 16px; "
        "}"
    );
    return frame;
}

void MainWindow::updateIndicatorState(QFrame *frame, QLabel *label, bool active, const QString &text)
{
    if (active) {
        frame->setStyleSheet(
            "QFrame { "
            "  background-color: #2ecc71; "
            "  border-radius: 8px; "
            "  min-width: 16px; "
            "  max-width: 16px; "
            "  min-height: 16px; "
            "  max-height: 16px; "
            "}"
        );
        label->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #27ae60; padding: 5px; }");
    } else {
        frame->setStyleSheet(
            "QFrame { "
            "  background-color: #95a5a6; "
            "  border-radius: 8px; "
            "  min-width: 16px; "
            "  max-width: 16px; "
            "  min-height: 16px; "
            "  max-height: 16px; "
            "}"
        );
        label->setStyleSheet("QLabel { font-size: 14px; font-weight: bold; color: #7f8c8d; padding: 5px; }");
    }
    label->setText(text);
}

void MainWindow::updateStatusIndicator(bool running, bool clientConnected)
{
    if (clientConnected) {
        statusIndicator->setStyleSheet("QLabel { color: #2ecc71; font-size: 32px; padding: 0 10px; }");
        statusIndicator->setText("●");
    } else if (running) {
        statusIndicator->setStyleSheet("QLabel { color: #f39c12; font-size: 32px; padding: 0 10px; }");
        statusIndicator->setText("●");
    } else {
        statusIndicator->setStyleSheet("QLabel { color: #95a5a6; font-size: 32px; padding: 0 10px; }");
        statusIndicator->setText("●");
    }
}


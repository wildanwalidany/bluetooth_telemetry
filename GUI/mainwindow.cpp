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
      totalBytes(0)
{
    setWindowTitle("Bluetooth Telemetry RFCOMM Server");
    setGeometry(100, 100, 1000, 800);
    
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
    
    // Control panel
    QGroupBox *controlGroup = new QGroupBox("Server Control", this);
    QHBoxLayout *controlLayout = new QHBoxLayout(controlGroup);
    
    startButton = new QPushButton("Start Server", this);
    stopButton = new QPushButton("Stop Server", this);
    stopButton->setEnabled(false);
    echoModeCheck = new QCheckBox("Echo Mode", this);
    hexModeCheck = new QCheckBox("Hex Output", this);
    hexModeCheck->setChecked(true);
    
    controlLayout->addWidget(startButton);
    controlLayout->addWidget(stopButton);
    controlLayout->addWidget(echoModeCheck);
    controlLayout->addWidget(hexModeCheck);
    controlLayout->addStretch();
    
    connect(startButton, &QPushButton::clicked, this, &MainWindow::onStartServer);
    connect(stopButton, &QPushButton::clicked, this, &MainWindow::onStopServer);
    
    mainLayout->addWidget(controlGroup);
    
    // Status
    QGroupBox *statusGroup = new QGroupBox("Connection Status", this);
    QVBoxLayout *statusLayout = new QVBoxLayout(statusGroup);
    statusLabel = new QLabel("Server: Stopped", this);
    clientAddressLabel = new QLabel("Client: None", this);
    QFont boldFont;
    boldFont.setBold(true);
    statusLabel->setFont(boldFont);
    clientAddressLabel->setFont(boldFont);
    statusLayout->addWidget(statusLabel);
    statusLayout->addWidget(clientAddressLabel);
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
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    logOutput = new QTextEdit(this);
    logOutput->setReadOnly(true);
    logOutput->setMaximumHeight(200);
    logLayout->addWidget(logOutput);
    mainLayout->addWidget(logGroup);
}

void MainWindow::createTelemetryGroup(QGroupBox *&group)
{
    group = new QGroupBox("Telemetry Data", this);
    QGridLayout *layout = new QGridLayout(group);
    
    int row = 0;
    
    // Speed
    layout->addWidget(new QLabel("<b>Speed:</b>", this), row, 0);
    speedLabel = new QLabel("-- RPM", this);
    layout->addWidget(speedLabel, row++, 1);
    
    // Throttle
    layout->addWidget(new QLabel("<b>Throttle:</b>", this), row, 0);
    throttleLabel = new QLabel("--", this);
    layout->addWidget(throttleLabel, row++, 1);
    
    // Odometer
    layout->addWidget(new QLabel("<b>Odometer:</b>", this), row, 0);
    odometerLabel = new QLabel("-- km", this);
    layout->addWidget(odometerLabel, row++, 1);
    
    // Battery
    layout->addWidget(new QLabel("<b>Battery:</b>", this), row, 0);
    batteryLabel = new QLabel("--%", this);
    layout->addWidget(batteryLabel, row++, 1);
    
    // Engine Temp
    layout->addWidget(new QLabel("<b>Engine Temp:</b>", this), row, 0);
    engineTempLabel = new QLabel("-- °C", this);
    layout->addWidget(engineTempLabel, row++, 1);
    
    // Battery Temp
    layout->addWidget(new QLabel("<b>Battery Temp:</b>", this), row, 0);
    batteryTempLabel = new QLabel("--", this);
    layout->addWidget(batteryTempLabel, row++, 1);
    
    row = 0;
    // State
    layout->addWidget(new QLabel("<b>State:</b>", this), row, 2);
    stateLabel = new QLabel("--", this);
    layout->addWidget(stateLabel, row++, 3);
    
    // Mode
    layout->addWidget(new QLabel("<b>Mode:</b>", this), row, 2);
    modeLabel = new QLabel("--", this);
    layout->addWidget(modeLabel, row++, 3);
    
    // Turn Signal
    layout->addWidget(new QLabel("<b>Turn Signal:</b>", this), row, 2);
    turnSignalLabel = new QLabel("--", this);
    layout->addWidget(turnSignalLabel, row++, 3);
    
    // Night Mode
    layout->addWidget(new QLabel("<b>Night Mode:</b>", this), row, 2);
    nightModeLabel = new QLabel("--", this);
    layout->addWidget(nightModeLabel, row++, 3);
    
    // High Beam
    layout->addWidget(new QLabel("<b>High Beam:</b>", this), row, 2);
    beamLabel = new QLabel("--", this);
    layout->addWidget(beamLabel, row++, 3);
    
    // Horn
    layout->addWidget(new QLabel("<b>Horn:</b>", this), row, 2);
    hornLabel = new QLabel("--", this);
    layout->addWidget(hornLabel, row++, 3);
    
    // Alert
    layout->addWidget(new QLabel("<b>Alert:</b>", this), row, 2);
    alertLabel = new QLabel("--", this);
    layout->addWidget(alertLabel, row++, 3);
    
    // Maps
    layout->addWidget(new QLabel("<b>Maps:</b>", this), row, 2);
    mapsLabel = new QLabel("--", this);
    layout->addWidget(mapsLabel, row++, 3);
}

void MainWindow::createStatsGroup(QGroupBox *&group)
{
    group = new QGroupBox("Statistics", this);
    QHBoxLayout *layout = new QHBoxLayout(group);
    
    layout->addWidget(new QLabel("<b>Messages:</b>", this));
    msgCountLabel = new QLabel("0", this);
    layout->addWidget(msgCountLabel);
    
    layout->addWidget(new QLabel("<b>Total Bytes:</b>", this));
    totalBytesLabel = new QLabel("0", this);
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
        
        statusLabel->setText("Server: Running on channel 1");
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
    
    statusLabel->setText("Server: Stopped");
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
    
    speedLabel->setText(QString("%1 RPM").arg(telem->speed * 46));
    throttleLabel->setText(QString::number(telem->throttle));
    odometerLabel->setText(QString("%1 km").arg(telem->total_miles * 1.60934, 0, 'f', 1));
    batteryLabel->setText(QString("%1%").arg(telem->battery));
    engineTempLabel->setText(QString("%1 °C").arg((int)telem->engine_temp - 20));
    batteryTempLabel->setText(QString::number(telem->battery_temp));
    stateLabel->setText(state_str[telem->state]);
    modeLabel->setText(mode_str[telem->mode]);
    turnSignalLabel->setText(signal_str[telem->turn_signal]);
    nightModeLabel->setText(telem->night_mode ? "ON" : "OFF");
    beamLabel->setText(telem->beam ? "ON" : "OFF");
    hornLabel->setText(telem->horn ? "ON" : "OFF");
    alertLabel->setText(QString::number(telem->alert));
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QCheckBox>
#include <QGroupBox>
#include <QSocketNotifier>
#include <QTimer>
#include <stdint.h>

/* Telemetry data structure */
typedef struct {
    uint8_t speed;           // B0: 0-255 (rpm/46)
    uint8_t throttle;        // B1: 0-255
    uint16_t total_miles;    // B2-B3: 16-bit odometer
    uint8_t battery;         // B4[0-6]: 0-100%
    uint8_t night_mode;      // B4[7]: 0/1
    uint8_t engine_temp;     // B5[0-5]: 0-63 (offset -20°C)
    uint8_t turn_signal;     // B5[6-7]: 0=none, 1=right, 2=left, 3=hazard
    uint8_t battery_temp;    // B6[0-5]: 0-63
    uint8_t horn;            // B6[6]: 0/1
    uint8_t beam;            // B6[7]: 0/1
    uint8_t alert;           // B7[0-2]: 0-7
    uint8_t state;           // B7[3-4]: 1=N, 2=D, 3=P
    uint8_t mode;            // B7[5-6]: 1=ECON, 2=COMF, 3=SPORT
    uint8_t maps;            // B7[7]: 0/1
} telemetry_t;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartServer();
    void onStopServer();
    void onServerSocketReady();
    void onClientSocketReady();
    void checkClientConnection();

private:
    // UI Components
    QPushButton *startButton;
    QPushButton *stopButton;
    QCheckBox *echoModeCheck;
    QCheckBox *hexModeCheck;
    QTextEdit *logOutput;
    
    // Telemetry display labels
    QLabel *statusLabel;
    QLabel *clientAddressLabel;
    QLabel *speedLabel;
    QLabel *throttleLabel;
    QLabel *odometerLabel;
    QLabel *batteryLabel;
    QLabel *engineTempLabel;
    QLabel *batteryTempLabel;
    QLabel *stateLabel;
    QLabel *modeLabel;
    QLabel *turnSignalLabel;
    QLabel *nightModeLabel;
    QLabel *beamLabel;
    QLabel *hornLabel;
    QLabel *alertLabel;
    QLabel *mapsLabel;
    QLabel *msgCountLabel;
    QLabel *totalBytesLabel;
    
    // Bluetooth server state
    int serverSocket;
    int clientSocket;
    QSocketNotifier *serverNotifier;
    QSocketNotifier *clientNotifier;
    QTimer *clientCheckTimer;
    bool isRunning;
    bool echoMode;
    bool hexMode;
    unsigned long msgCount;
    unsigned long totalBytes;
    
    // Helper methods
    void setupUI();
    void createTelemetryGroup(QGroupBox *&group);
    void createStatsGroup(QGroupBox *&group);
    bool startBluetoothServer();
    void stopBluetoothServer();
    void acceptClientConnection();
    void handleClientData();
    int parseTelemetry(const uint8_t *data, size_t len, telemetry_t *telem);
    void displayTelemetry(const telemetry_t *telem);
    void logMessage(const QString &msg);
    void logHex(const uint8_t *data, size_t len);
    QString getTimestamp();
};

#endif // MAINWINDOW_H

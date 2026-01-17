# Bluetooth Telemetry Server GUI

Qt-based GUI application for the Bluetooth RFCOMM telemetry server.

## Features
- Real-time telemetry data display
- Visual dashboard showing all vehicle parameters
- Connection status monitoring
- Configurable echo and hex output modes
- Live server logs
- Message count and byte statistics

## Requirements
- Qt6 (Core, Gui, Widgets)
- BlueZ libraries (`libbluetooth-dev`)
- CMake 3.16+ or qmake
- GCC compiler
- Root privileges for Bluetooth operations

## Installation

### Install Dependencies
```sh
sudo apt-get update
sudo apt-get install qt6-base-dev libbluetooth-dev build-essential cmake
```

### Build with CMake
```sh
cd GUI
mkdir build
cd build
cmake ..
make
```

### Build with qmake
```sh
cd GUI
qmake BluetoothTelemetryGUI.pro
make
```

## Usage

### Run the Server
The application requires root privileges for Bluetooth access:
```sh
sudo ./BluetoothTelemetryGUI
```

Or from build directory:
```sh
sudo ./build/BluetoothTelemetryGUI
```

### Operating the GUI
1. **Start Server**: Click "Start Server" to begin listening for Bluetooth connections
2. **Configure Options**: 
   - Enable "Echo Mode" to echo received data back to client
   - Enable "Hex Output" to display raw hex data in logs
3. **Monitor Connection**: View connected client MAC address in status panel
4. **View Telemetry**: Real-time vehicle data appears in the telemetry panel
5. **Check Logs**: Server events and data are logged in the log panel
6. **Stop Server**: Click "Stop Server" to shutdown the server

### Bluetooth Setup
Before connecting a client, ensure Bluetooth is properly configured:
```sh
sudo hciconfig hci0 up
sudo hciconfig hci0 piscan
sudo sdptool add SP
```

## Telemetry Display
The GUI displays the following parameters:
- **Speed**: Motor RPM
- **Throttle**: 0-255
- **Odometer**: Total distance in km
- **Battery**: 0-100%
- **Engine Temperature**: °C
- **Battery Temperature**: Raw value
- **State**: N (Neutral), D (Drive), P (Park)
- **Mode**: ECON, COMF, SPORT
- **Turn Signal**: none, right, left, hazard
- **Night Mode**: ON/OFF
- **High Beam**: ON/OFF
- **Horn**: ON/OFF
- **Alert**: 0-7
- **Maps**: ON/OFF

## Architecture
- **mainwindow.h/cpp**: Main UI and Bluetooth logic
- **main.cpp**: Application entry point
- **QSocketNotifier**: Async I/O for Bluetooth sockets
- **RFCOMM**: Bluetooth serial protocol on channel 1

## Troubleshooting

### Permission Denied
Run with sudo: `sudo ./BluetoothTelemetryGUI`

### Qt6 Not Found
Install Qt6 development packages or use Qt5 by changing `Qt6` to `Qt5` in CMakeLists.txt

### Bluetooth Connection Issues
- Ensure devices are paired and trusted
- Check that `sdptool add SP` was executed
- Verify Bluetooth adapter is up: `hciconfig hci0 up`
- Set discoverable mode: `hciconfig hci0 piscan`

### Build Errors
Make sure all dependencies are installed:
```sh
sudo apt-get install qt6-base-dev qt6-base-dev-tools libbluetooth-dev
```

## License
MIT

# Bluetooth Telemetry GUI

Qt6-based GUI for the Bluetooth RFCOMM telemetry server.

## Dependencies

```sh
sudo apt-get install qt6-base-dev libbluetooth-dev build-essential cmake
```

## Build

```sh
cd GUI
mkdir build && cd build
cmake ..
make
```

## Run

Bluetooth requires root privileges:

```sh
sudo ./build/BluetoothTelemetryGUI
```

Before connecting a client, make sure Bluetooth is ready:

```sh
sudo hciconfig hci0 up
sudo hciconfig hci0 piscan
sudo sdptool add SP
```

## Usage

1. Click **Start Server** to begin listening for connections.
2. Connect the Bluetooth client — telemetry data will appear automatically.
3. Click **Stop Server** to shut down.

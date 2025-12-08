# Bluetooth Telemetry Project

This project provides a simple Bluetooth RFCOMM server and client for sending and receiving telemetry data.

## Contents
- `bt-client.c`: Bluetooth client that sends telemetry frames
- `rfcomm_server_v2.c`: Bluetooth RFCOMM server that receives and parses telemetry frames
- `rfcomm_server.c`: Basic RFCOMM server (prints raw data)

## Requirements
- Linux system with Bluetooth support
- BlueZ libraries (`libbluetooth-dev`)
- GCC compiler
- Root privileges for running Bluetooth programs

## Installation

1. **Install dependencies:**
   ```sh
   sudo apt-get update
   sudo apt-get install libbluetooth-dev build-essential
   ```

2. **Compile the server and client:**
   ```sh
   gcc -o rfcomm_server_v2 rfcomm_server_v2.c -lbluetooth
   gcc -o bt-client bt-client.c -lbluetooth
   ```

## Usage

### 1. Start the RFCOMM Server
Run the server on your Linux machine:
```sh
sudo ./rfcomm_server_v2 -x
```
- `-x` enables hex output (optional)
- `-e` enables echo mode (optional)

### 2. Prepare Bluetooth on the Server
Use `bluetoothctl` to make your device discoverable and pairable:
```sh
bluetoothctl
power on
agent on
default-agent
pairable on
discoverable on
scan on
```
Pair and trust your client device:
```
pair XX:XX:XX:XX:XX:XX
trust XX:XX:XX:XX:XX:XX
connect XX:XX:XX:XX:XX:XX
```

### 3. Run the Client
On the client device (or same machine):
```sh
sudo ./bt-client --addr <SERVER_MAC> --channel 1 --verbose
```
Replace `<SERVER_MAC>` with your server's Bluetooth MAC address.

### 4. Telemetry Data
- The client sends packed telemetry frames every 150ms (default).
- The server parses and displays the telemetry in a readable format.

## Troubleshooting
- Make sure both devices are paired and trusted.
- Run programs as root (`sudo`) for Bluetooth access.
- Check that `libbluetooth-dev` is installed.
- Use `rfcomm_server.c` for raw data debugging.

## License
MIT

#!/bin/bash

# Tunggu bluetoothd siap
sleep 2

# Pastikan adapter hidup
bluetoothctl <<EOF
power on
agent on
default-agent
pairable on
discoverable on
EOF

# Pastikan SP terdaftar
sudo sdptool add SP


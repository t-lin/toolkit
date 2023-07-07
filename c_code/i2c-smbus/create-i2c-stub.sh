#!/bin/bash
# This script instantiates a fake I2C/SMBus device (i2c-stub) with a chip
# address of 0x03. This fake device is needed by the i2c_utils test.

sudo modprobe i2c-stub chip_addr=0x03

# Identify I2C stub device (assumes devices are under /dev/)
I2C_DEV=/dev/$(i2cdetect -l | grep 'SMBus stub driver' | awk '{print $1}')
if [[ ! -c ${I2C_DEV} ]]; then
    echo "ERROR: Unable to find i2c-stub device ${I2C_DEV}"
    exit 1
fi

echo "i2c-stub device instantiated at ${I2C_DEV}"
sudo chmod o+rw ${I2C_DEV}

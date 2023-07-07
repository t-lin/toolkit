# I2C utils and tests
*NOTE:* Before running the tests, run the `create-i2c-stub.sh` script to create a fake I2C/SMBus device

The script will instantiate a fake I2C/SMBus device (i2c-stub) with a chip address of 0x03.
This fake device is needed by the tests.

If you change the chip address of the fake device, you'll need to also change the code (specifically, the `I2C_STUB_DEV_ADDR` macro).


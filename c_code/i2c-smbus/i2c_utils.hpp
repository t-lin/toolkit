// This file includes code from a public repo as well as custom-made code

#pragma once
#ifndef _LIB_I2C_H_
#define _LIB_I2C_H_

#include <sys/types.h>
#include <stdint.h>

extern "C" {  // The i2c libraries still haven't been "C++-ified"
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
}

namespace I2CUtils {
/**********************************************************************
* The following is custom code from: https://github.com/amaork/libi2c
**********************************************************************/

/* I2c device */
typedef struct i2c_device {
    /* I2C Bus fd, return from i2c_open */
    int bus = -1;

    /* I2C device(slave) address */
    unsigned short addr = 0;

    /* I2C is 10 bit device address */
    unsigned char tenbit = 0;

    /* I2C internal operation delay, unit millisecond */
    unsigned char delay = 0;

    /* I2C i2c_ioctl_read/write flags */
    unsigned short flags = 0;

    /* I2C max number of bytes per page
     * e.g. 1K/2K 8, 4K/8K/16K 16, 32K/64K 32, etc
     */
    unsigned int page_bytes = 0;

    /* I2C device internal (word) address bytes
     * e.g. 24C04 1 byte, 24C64 2 bytes
     */
    unsigned int iaddr_bytes = 0;
} I2CDevice;

/* Close i2c bus */
void i2c_close(int bus);

/* Open i2c bus, return i2c bus fd */
int i2c_open(const char *bus_name);

/* Initialize I2CDevice with default value */
void i2c_init_device(I2CDevice *device);

/* Get i2c device description */
char *i2c_get_device_desc(const I2CDevice *device, char *buf, size_t size);

/* Select i2c device on i2c bus */
int i2c_select(int bus, unsigned long dev_addr, unsigned long tenbit);

/* I2C internal(word) address convert */
void i2c_iaddr_convert(unsigned int int_addr,
                       unsigned int iaddr_bytes, unsigned char *addr);

/* I2C file I/O read, write */
ssize_t i2c_read(const I2CDevice *device, unsigned int iaddr,
                 void *buf, size_t len);
ssize_t i2c_write(const I2CDevice *device, unsigned int iaddr,
                  const void *buf, size_t len);

/* I2c ioctl read, write can set i2c flags */
ssize_t i2c_ioctl_read(const I2CDevice *device, unsigned int iaddr,
                       void *buf, size_t len);
ssize_t i2c_ioctl_write(const I2CDevice *device, unsigned int iaddr,
                        const void *buf, size_t len);

/* I2C read / write handle function */
typedef ssize_t (*I2C_READ_HANDLE)(const I2CDevice *dev, unsigned int iaddr,
                                   void *buf, size_t len);
typedef ssize_t (*I2C_WRITE_HANDLE)(const I2CDevice *dev, unsigned int iaddr,
                                    const void *buf, size_t len);

/******************************************************************
 * The following is custom code which adds methods
 * for SMBus-based devices.
 ******************************************************************/

/**
 * @brief Open a handle to an I2C SMBus.
 *
 * @param bus_name Path to the I2C SMBus in the filesystem.
 * @param dev_addr Address of the I2C device.
 * @param i2c_dev Pointer to an I2CDevice structure which will be initialized
 *                and populated by this function.
 *
 * @return Returns true if opening the device succeeded, otherwise false.
 */
bool i2c_smbus_open(const char *bus_name, uint16_t dev_addr,
                    I2CDevice* i2c_dev);

/**
 * @brief Closes the handle to an I2C SMBus.
 *        Will reset the handle's 'bus' and 'addr' member variables
 *        to -1 and 0, respectively.
 *
 * @param bus Pointer to a valid I2CDevice structure.
 */
void i2c_smbus_close(I2CDevice* i2c_dev);

/**
 * @brief Reads a single byte from a register on an I2C device
 *
 * @param device I2CDevice structure containing an open file descriptor to
 *               the bus the device is on, and the address of the device.
 * @param reg The address of the register on the device.
 *
 * @return Returns the retrieved byte, or negative errno on error.
 */
int32_t i2c_smbus_read_uint8(const I2CDevice* device, uint8_t reg);

/**
 * @brief Writes a single byte to a register on an I2C device.
 *
 * @param device I2CDevice structure containing an open file descriptor to
 *               the bus the device is on, and the address of the device.
 * @param reg The address of the register on the device.
 * @param val The value to write to the register.
 *
 * @return Returns 0 if successfully written, or negative errno on error.
 */
int32_t i2c_smbus_write_uint8(const I2CDevice* device,
                              uint8_t reg, uint8_t val);

/**
 * @brief Reads a 16-bit short from a register on an I2C device
 *
 * @param device I2CDevice structure containing an open file descriptor to
 *               the bus the device is on, and the address of the device.
 * @param reg The address of the register on the device.
 *
 * @return Returns the retrieved short, or negative errno on error.
 */
int32_t i2c_smbus_read_uint16(const I2CDevice* device, uint8_t reg);

/**
 * @brief Writes a 16-bit short to a register on an I2C device.
 *
 * @param device I2CDevice structure containing an open file descriptor to
 *               the bus the device is on, and the address of the device.
 * @param reg The address of the register on the device.
 * @param val The value to write to the register.
 *
 * @return Returns 0 if successfully written, or negative errno on error.
 */
int32_t i2c_smbus_write_uint16(const I2CDevice* device,
                               uint8_t reg, uint16_t val);

/**
 * @brief Reads a block of data from a register on an I2C device
 *
 * @param device I2CDevice structure containing an open file descriptor to
 *               the bus the device is on, and the address of the device.
 * @param reg The address of the register on the device.
 * @param buf Pointer to a buffer to store the data read from the device. The
 *            buffer should have at least I2C_SMBUS_BLOCK_MAX bytes allocated.
 *
 * @return Returns the number of bytes read, or negative errno on error.
 */
int32_t i2c_smbus_read_buffer(const I2CDevice* device,
                              uint8_t reg, uint8_t* buf);

/**
 * @brief Writes a block of data to a register on an I2C device.
 *
 * @param device I2CDevice structure containing an open file descriptor to
 *               the bus the device is on, and the address of the device.
 * @param reg The address of the register on the device.
 * @param buf Pointer to a buffer holding data to write to the device.
 * @param len Number of bytes from the buffer to write to the device. Note
 *            that only values up to I2C_SMBUS_BLOCK_MAX are allowed.
 *
 * @return Returns 0 if successfully written, or negative errno on error.
 */
int32_t i2c_smbus_write_buffer(const I2CDevice* device, uint8_t reg,
                               const uint8_t* buf, uint8_t len);
} // namespace I2CUtils

#endif


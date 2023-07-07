// This file includes code from a public repo as well as custom-made code
//
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <errno.h>

#include "i2c_utils.hpp"

#define SMBUS_ADDR_MAX (0x7F)

namespace I2CUtils {

/*******************************
 * Unexported helper functions
 *******************************/

/**
 * @brief Checks whether the I2CDevice handle (i.e. pointer) is valid.
 *        Prints an error message if the check fails.
 *
 * @param dev The I2CDevice handle.
 * @param func_name Name of function invoking this check.
 *
 * @return True if the handle is valid, false otherwise.
 */
inline bool checkI2CDevice(const I2CDevice* dev, const char* func_name) {
    if (dev == NULL) {
        fprintf(stderr, "ERROR: In %s, NULL I2CDevice\n", func_name);
        return false;
    }

    return true;
}

/**
 * @brief Checks whether the file descriptor is valid, where we define valid
 *        as non-negative and not stdin, stdout, and stderr.
 *        Prints an error message if the check fails.
 *
 * @param fd The file descriptor to check.
 * @param func_name Name of function invoking this check.
 *
 * @return True if the file descriptor is valid, false otherwise.
 */
inline bool checkFileDesc(int fd, const char* func_name) {
    if (fd < 3) { // 3 because we don't allow stdin, stdout, and stderr
        fprintf(stderr, "ERROR: In %s, bad file descriptor\n", func_name);
        return false;
    }

    return true;
}

/**
 * @brief Checks the I2CDevice handle and its stored file descriptor for
 *        validity, and attempts to select the specified I2C slave device.
 *        Prints an error message if either check fails or if the device
 *        selection fails..
 *
 * @param dev The I2CDevice handle.
 * @param func_name Name of function invoking these checks & device selection.
 *
 * @return True if the device selection succeeds, false otherwise.
 */
inline bool checkSelectDev(const I2CDevice* dev, const char* func_name) {
    if (checkI2CDevice(dev, func_name) == false ||
        checkFileDesc(dev->bus, func_name) == false) {
        return false;
    }

    /* Set i2c slave address */
    if (i2c_select(dev->bus, dev->addr, dev->tenbit) < 0) {
        fprintf(stderr, "ERROR: In %s, unable to select device %hx\n",
                func_name, dev->addr);
        return false;
    }

    return true;
}

/**********************************************************************
 * The following is custom code from: https://github.com/amaork/libi2c
 **********************************************************************/

/* I2C default delay */
#define I2C_DEFAULT_DELAY 1

/* I2C internal address max length */
#define INT_ADDR_MAX_BYTES 4

/* I2C page max bytes */
#define PAGE_MAX_BYTES 4096

#define GET_I2C_DELAY(delay) ((delay) == 0 ? I2C_DEFAULT_DELAY : (delay))
#define GET_I2C_FLAGS(tenbit, flags) \
    ((tenbit) ? ((flags) | I2C_M_TEN) : (flags))
#define GET_WRITE_SIZE(addr, remain, page_bytes) \
    ((addr) + (remain) > (page_bytes) ? (page_bytes) - (addr) : (remain))

static void i2c_delay(unsigned char delay);

/*
**    @brief      :    Open i2c bus
**    #bus_name   :    i2c bus name such as: /dev/i2c-1
**    @return     :    failed return -1, success return i2c bus fd
*/
int i2c_open(const char *bus_name)
{
    int fd;

    /* Open i2c-bus devcice */
    if ((fd = open(bus_name, O_RDWR)) == -1) {

        return -1;
    }

    return fd;
}


void i2c_close(int bus)
{
    close(bus);
}


/*
**    @brief        :    Initialize I2CDevice with defualt value
**    #device       :    I2CDevice struct
*/
void i2c_init_device(I2CDevice *device)
{
    /* 7 bit device address */
    device->tenbit = 0;

    /* 1ms delay */
    device->delay = 1;

    /* 8 bytes per page */
    device->page_bytes = 8;

    /* 1 byte internal(word) address */
    device->iaddr_bytes = 1;
}


/*
**    @brief      :   Get I2CDevice struct desc
**    #device     :   I2CDevice struct
**    #buf        :   Description message buffer
**    #size       :   #buf size
**    @return     :   return i2c device desc
*/
char *i2c_get_device_desc(const I2CDevice *device, char *buf, size_t size)
{
    memset(buf, 0, size);
    snprintf(buf, size,
             "Device address: 0x%x, tenbit: %s, internal(word) address: "
             "%d bytes, page max %d bytes, delay: %dms",
             device->addr, device->tenbit ? "True" : "False",
             device->iaddr_bytes, device->page_bytes, device->delay);

    return buf;
}


/*
**    i2c_ioctl_read/write
**    I2C bus top layer interface to operation different i2c devide
**    This function will call XXX:ioctl system call and will be related
**    i2c-dev.c i2cdev_ioctl to operation i2c device.
**    1. it can choice ignore or not ignore i2c bus ack signal
         (flags set I2C_M_IGNORE_NAK)
**    2. it can choice ignore or not ignore i2c internal address
**
*/
ssize_t i2c_ioctl_read(const I2CDevice *device, unsigned int iaddr,
                       void *buf, size_t len)
{
    struct i2c_msg ioctl_msg[2];
    struct i2c_rdwr_ioctl_data ioctl_data;
    unsigned char addr[INT_ADDR_MAX_BYTES];
    unsigned short flags = GET_I2C_FLAGS(device->tenbit, device->flags);

    memset(addr, 0, sizeof(addr));
    memset(ioctl_msg, 0, sizeof(ioctl_msg));
    memset(&ioctl_data, 0, sizeof(ioctl_data));

    /* Target have internal address */
    if (device->iaddr_bytes) {

        i2c_iaddr_convert(iaddr, device->iaddr_bytes, addr);

        /* First message is write internal address */
        ioctl_msg[0].len    =    device->iaddr_bytes;
        ioctl_msg[0].addr    =     device->addr;
        ioctl_msg[0].buf    =     addr;
        ioctl_msg[0].flags    =     flags;

        /* Second message is read data */
        ioctl_msg[1].len    =     len;
        ioctl_msg[1].addr    =     device->addr;
        ioctl_msg[1].buf    =    (uint8_t*)buf;
        ioctl_msg[1].flags    =    flags | I2C_M_RD;

        /* Package to i2c message to operation i2c device */
        ioctl_data.nmsgs    =    2;
        ioctl_data.msgs        =    ioctl_msg;
    }
    /* Target did not have internal address */
    else {

        /* Direct send read data message */
        ioctl_msg[0].len    =     len;
        ioctl_msg[0].addr    =     device->addr;
        ioctl_msg[0].buf    =    (uint8_t*)buf;
        ioctl_msg[0].flags    =    flags | I2C_M_RD;

        /* Package to i2c message to operation i2c device */
        ioctl_data.nmsgs    =    1;
        ioctl_data.msgs        =    ioctl_msg;
    }

    /* Using ioctl interface operation i2c device */
    if (ioctl(device->bus, I2C_RDWR, (unsigned long)&ioctl_data) == -1) {

        perror("Ioctl read i2c error:");
        return -1;
    }

    return len;
}


ssize_t i2c_ioctl_write(const I2CDevice *device, unsigned int iaddr,
                        const void *buf, size_t len)
{
    ssize_t remain = len;
    size_t size = 0, cnt = 0;
    const unsigned char *buffer = (const unsigned char*)buf;
    unsigned char delay = GET_I2C_DELAY(device->delay);
    unsigned short flags = GET_I2C_FLAGS(device->tenbit, device->flags);

    struct i2c_msg ioctl_msg;
    struct i2c_rdwr_ioctl_data ioctl_data;
    unsigned char tmp_buf[PAGE_MAX_BYTES + INT_ADDR_MAX_BYTES];

    while (remain > 0) {

        size = GET_WRITE_SIZE(iaddr % device->page_bytes,
                              remain, device->page_bytes);

        /* Convert i2c internal address */
        memset(tmp_buf, 0, sizeof(tmp_buf));
        i2c_iaddr_convert(iaddr, device->iaddr_bytes, tmp_buf);

        /* Connect write data after device internal address */
        memcpy(tmp_buf + device->iaddr_bytes, buffer, size);

        /* Fill kernel ioctl i2c_msg */
        memset(&ioctl_msg, 0, sizeof(ioctl_msg));
        memset(&ioctl_data, 0, sizeof(ioctl_data));

        ioctl_msg.len    =    device->iaddr_bytes + size;
        ioctl_msg.addr    =    device->addr;
        ioctl_msg.buf    =    tmp_buf;
        ioctl_msg.flags    =    flags;

        ioctl_data.nmsgs =    1;
        ioctl_data.msgs    =    &ioctl_msg;

        if (ioctl(device->bus, I2C_RDWR, (unsigned long)&ioctl_data) == -1) {

            perror("Ioctl write i2c error:");
            return -1;
        }

        /* XXX: Must have a little time delay */
        i2c_delay(delay);

        cnt += size;
        iaddr += size;
        buffer += size;
        remain -= size;
    }

    return cnt;
}


/*
**    @brief  :    read #len bytes data from #device #iaddr to #buf
**    #device :    I2CDevice struct, must call i2c_device_init first
**    #iaddr  :    i2c_device internal address will read data from this
                   address, no address set zero
**    #buf    :    i2c data will read to here
**    #len    :    how many data to read, lenght must less than or
                   equal to buf size
**    @return :    success return read data length, failed -1
*/
ssize_t i2c_read(const I2CDevice *device, unsigned int iaddr,
                 void *buf, size_t len)
{
    ssize_t cnt;
    unsigned char addr[INT_ADDR_MAX_BYTES];
    unsigned char delay = GET_I2C_DELAY(device->delay);

    /* Set i2c slave address */
    if (i2c_select(device->bus, device->addr, device->tenbit) == -1) {

        return -1;
    }

    /* Convert i2c internal address */
    memset(addr, 0, sizeof(addr));
    i2c_iaddr_convert(iaddr, device->iaddr_bytes, addr);

    /* Write internal address to devide  */
    if (write(device->bus, addr, device->iaddr_bytes) != device->iaddr_bytes) {

        perror("Write i2c internal address error");
        return -1;
    }

    /* Wait a while */
    i2c_delay(delay);

    /* Read count bytes data from int_addr specify address */
    if ((cnt = read(device->bus, buf, len)) == -1) {

        perror("Read i2c data error");
        return -1;
    }

    return cnt;
}


/*
**    @brief    :    write #buf data to i2c #device #iaddr address
**    #device   :    I2CDevice struct, must call i2c_device_init first
**    #iaddr    :     i2c_device internal address, no address set zero
**    #buf      :    data will write to i2c device
**    #len      :    buf data length without '/0'
**    @return   :     success return write data length, failed -1
*/
ssize_t i2c_write(const I2CDevice *device, unsigned int iaddr,
                  const void *buf, size_t len)
{
    ssize_t remain = len;
    ssize_t ret = -1;
    size_t cnt = 0, size = 0;
    const unsigned char *buffer = (const unsigned char*)buf;
    unsigned char delay = GET_I2C_DELAY(device->delay);
    unsigned char tmp_buf[PAGE_MAX_BYTES + INT_ADDR_MAX_BYTES];

    /* Set i2c slave address */
    if (i2c_select(device->bus, device->addr, device->tenbit) == -1) {

        return -1;
    }

    /* Once only can write less than 4 byte */
    while (remain > 0) {

        size = GET_WRITE_SIZE(iaddr % device->page_bytes, remain, device->page_bytes);

        /* Convert i2c internal address */
        memset(tmp_buf, 0, sizeof(tmp_buf));
        i2c_iaddr_convert(iaddr, device->iaddr_bytes, tmp_buf);

        /* Copy data to tmp_buf */
        memcpy(tmp_buf + device->iaddr_bytes, buffer, size);

        /* Write to buf content to i2c device length  is address length and
                write buffer length */
        ret = write(device->bus, tmp_buf, device->iaddr_bytes + size);
        if (ret == -1 || (size_t)ret != device->iaddr_bytes + size)
        {
            perror("I2C write error:");
            return -1;
        }

        /* XXX: Must have a little time delay */
        i2c_delay(delay);

        /* Move to next #size bytes */
        cnt += size;
        iaddr += size;
        buffer += size;
        remain -= size;
    }

    return cnt;
}


/*
**    @brief    :    i2c internal address convert
**    #iaddr    :    i2c device internal address
**    #len      :    i2c device internal address length
**    #addr     :    save convert address
*/
void i2c_iaddr_convert(unsigned int iaddr,
                       unsigned int len, unsigned char *addr)
{
    union {
        unsigned int iaddr;
        unsigned char caddr[INT_ADDR_MAX_BYTES];
    } convert;

    /* I2C internal address order is big-endian, same with network order */
    convert.iaddr = htonl(iaddr);

    /* Copy address to addr buffer */
    int i = len - 1;
    int j = INT_ADDR_MAX_BYTES - 1;

    while (i >= 0 && j >= 0) {

        addr[i--] = convert.caddr[j--];
    }
}


/*
**    @brief        :    Select i2c address @i2c bus
**    #bus          :    i2c bus fd
**    #dev_addr     :    i2c device address
**    #tenbit       :    i2c device address is tenbit
**    #return       :    success return 0, failed return -1
*/
int i2c_select(int bus, unsigned long dev_addr, unsigned long tenbit)
{
    /* Set i2c device address bit */
    if (ioctl(bus, I2C_TENBIT, tenbit)) {

        perror("Set I2C_TENBIT failed");
        return -1;
    }

    /* Set i2c device as slave ans set it address */
    if (ioctl(bus, I2C_SLAVE, dev_addr)) {

        perror("Set i2c device address failed");
        return -1;
    }

    return 0;
}

/*
**    @brief    :    i2c delay
**    #msec     :    milliscond to be delay
*/
static void i2c_delay(unsigned char msec)
{
    usleep(msec * 1e3);
}

/******************************************************************
 * The following is custom code which adds methods
 * for devices that only support SMBus.
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
                    I2CDevice* i2c_dev) {
    if (bus_name == NULL) {
        fprintf(stderr, "ERROR: NULL bus name\n");
        return false;
    } else if (checkI2CDevice(i2c_dev, __func__) == false) {
        return false;
    }

    // Ensure address is 0x0 to 0x7F (SMBus doesn't allow 10-bit addresses)
    if (dev_addr > SMBUS_ADDR_MAX) {
        fprintf(stderr, "ERROR: Specified device address (0x%02hX) > "
                "SMBus' max address (0x%02X)\n", dev_addr, SMBUS_ADDR_MAX);
        return false;
    }

    int fd = i2c_open(bus_name);
    if (fd == -1) {
        fprintf(stderr, "ERROR: Unable to open bus %s\n", bus_name);
        return false;
    }

    i2c_init_device(i2c_dev);
    i2c_dev->bus = fd;
    i2c_dev->addr = dev_addr;

    return true;
}

/**
 * @brief Closes the handle to an I2C SMBus.
 *        Will reset the handle's 'bus' and 'addr' member variables
 *        to -1 and 0, respectively.
 *
 * @param bus Pointer to a valid I2CDevice structure.
 */
void i2c_smbus_close(I2CDevice* i2c_dev) {
    if (checkI2CDevice(i2c_dev, __func__) == false) {
        return;
    }

    i2c_close(i2c_dev->bus);
    i2c_dev->bus = -1;
    i2c_dev->addr = 0;
}

/**
 * @brief Reads a single byte from a register on an I2C device
 *
 * @param device I2CDevice structure containing an open file descriptor to
 *               the bus the device is on, and the address of the device.
 * @param reg The address of the register on the device.
 *
 * @return Returns the retrieved byte, or negative errno on error.
 */
int32_t i2c_smbus_read_uint8(const I2CDevice* device, uint8_t reg) {
    // Check device, file descriptor, and attempt to select device
    // Function prints error messages upon failure.
    if (checkSelectDev(device, __func__) == false) {
      return -1;
    }

    int32_t ret = i2c_smbus_read_byte_data(device->bus, reg);
    if (ret < 0) {
        fprintf(stderr, "ERROR: Unable to read byte from SMBus (%d): %s\n",
                errno, strerror(errno));
    }

    return ret;
}

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
                              uint8_t reg, uint8_t val) {
    // Check device, file descriptor, and attempt to select device
    // Function prints error messages upon failure.
    if (checkSelectDev(device, __func__) == false) {
        return -1;
    }

    int32_t ret = i2c_smbus_write_byte_data(device->bus, reg, val);
    if (ret < 0) {
        fprintf(stderr, "ERROR: Unable to write byte to SMBus (%d): %s\n",
                errno, strerror(errno));
    }

    return ret;
}

/**
 * @brief Reads a 16-bit short from a register on an I2C device
 *
 * @param device I2CDevice structure containing an open file descriptor to
 *               the bus the device is on, and the address of the device.
 * @param reg The address of the register on the device.
 *
 * @return Returns the retrieved short, or negative errno on error.
 */
int32_t i2c_smbus_read_uint16(const I2CDevice* device, uint8_t reg) {
    // Check device, file descriptor, and attempt to select device
    // Function prints error messages upon failure.
    if (checkSelectDev(device, __func__) == false) {
      return -1;
    }

    int32_t ret = i2c_smbus_read_word_data(device->bus, reg);
    if (ret < 0) {
        fprintf(stderr, "ERROR: Unable to read uint16 from SMBus (%d): %s\n",
                errno, strerror(errno));
    }

    return ret;
}

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
                               uint8_t reg, uint16_t val) {
    // Check device, file descriptor, and attempt to select device
    // Function prints error messages upon failure.
    if (checkSelectDev(device, __func__) == false) {
      return -1;
    }

    int32_t ret = i2c_smbus_write_word_data(device->bus, reg, val);
    if (ret < 0) {
        fprintf(stderr, "ERROR: Unable to write uint16 to SMBus (%d): %s\n",
                errno, strerror(errno));
    }

    return ret;
}

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
                              uint8_t reg, uint8_t* buf) {
    // Check device, file descriptor, and attempt to select device
    // Function prints error messages upon failure.
    if (checkSelectDev(device, __func__) == false) {
        return -1;
    } else if (buf == NULL) {
        fprintf(stderr, "ERROR: In %s, NULL buffer\n", __func__);
        return -1;
    }

    int32_t ret = i2c_smbus_read_block_data(device->bus, reg, buf);
    if (ret < 0) {
        fprintf(stderr, "ERROR: Unable to read block from SMBus (%d): %s\n",
                errno, strerror(errno));
    }

    return ret;
}

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
                               const uint8_t* buf, uint8_t len) {
    // Check device, file descriptor, and attempt to select device
    // Function prints error messages upon failure.
    if (checkSelectDev(device, __func__) == false) {
        return -1;
    } else if (buf == NULL) {
        fprintf(stderr, "ERROR: In %s, NULL buffer\n", __func__);
        return -1;
    }

    // Do a check on the length here and return appropriate error.
    // The implementation of i2c_smbus_write_block_data() will automatically
    // truncate lengths greater than I2C_SMBUS_BLOCK_MAX, which may mislead
    // users to believing their large messages have been written.
    if (len > I2C_SMBUS_BLOCK_MAX) {
        return -EMSGSIZE;
    }

    int32_t ret = i2c_smbus_write_block_data(device->bus, reg, len, buf);
    if (ret < 0) {
        fprintf(stderr, "ERROR: Unable to write block to SMBus (%d): %s\n",
                errno, strerror(errno));
    }

    return ret;
}

} // namespace I2CUtils

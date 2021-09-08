#!/bin/bash
# Shrinks main partition & zeroes out unused blocks of a disk image file
# Currently only works w/ disk images w/ 1 Linux filesystem, and only if
# that filesystem is ext4 and is placed as the final partition on the disk
# (regardless of its partition #)

if [[ $# -ne 1 ]]; then
    echo "ERROR: Must specify the name of the disk image to shrink"
    exit 1
else
    TARGET_IMG=$1
fi

if [[ "$EUID" -ne 0 ]]; then
    echo "Please run w/ sudo privileges or run as root user"
    exit 1
fi

# Install any pre-req tools
apt-get install gawk bc e2fsprogs qemu-utils util-linux gdisk

# Load nbd module, if it isn't already loaded
modprobe nbd
if [[ $? -ne 0 ]]; then
    echo "ERROR: Could not load nbd kernel module"
    exit 1
fi
NBD_DEV=/dev/nbd6 # Randomly chosen

# Connect disk image
qemu-nbd -c ${NBD_DEV} ${TARGET_IMG}
if [[ $? -ne 0 ]]; then
    echo "ERROR: Could not connect nbd device to image (make sure it's not in use)"
    exit 1
fi

# Just in case qemu-nbd doesn't automatically probe & refresh the partitions
partprobe ${NBD_DEV}

# Find target partition
FDISK_L=$(fdisk -l /dev/nbd6)
NUM_LINUX_FS=$(echo "${FDISK_L}" | grep "Linux filesystem" | wc -l)
if [[ ${NUM_LINUX_FS} -ne 1 ]]; then
    if [[ ${NUM_LINUX_FS} -gt 1 ]]; then
        echo "ERROR: More than one Linux filesystem on disk"
        echo "	 Currently, this script only supports disks w/ 1 filesystem"
        # TODO: In the future, ask user which FS partition to alter
    elif [[ ${NUM_LINUX_FS} -eq 0 ]]; then
        echo "ERROR: No Linux filesystems detected on disk"
    fi

    # Disconnect NBD from image
    qemu-nbd -d ${NBD_DEV}
    exit 1
fi

TARGET_PART=$(echo "${FDISK_L}" | grep "Linux filesystem" | awk '{print $1}')

# Shrink filesystem & zero out unused blocks
# Make multiple passes, each time the filesystem should get progressively smaller
# Limit the number of passes to prevent this process from taking too long
# Ideally this procedure is done on a fast SSD
TMP_MOUNT=$(mktemp -d)
NUM_PASSES=10 # Maximum number of attempts to continuously shrink
for ((i = 0; i < ${NUM_PASSES}; i++)); do
    e2fsck -f -p ${TARGET_PART}
    mount ${TARGET_PART} ${TMP_MOUNT}

    echo "========== PASS ${i}: Defragmenting..."
    e4defrag ${TARGET_PART} > /dev/null
    mount -o remount,ro ${TMP_MOUNT}

    echo "========== PASS ${i}: Zeroing out unused blocks..."
    zerofree ${TARGET_PART}
    umount ${TMP_MOUNT}
    e2fsck -f -p ${TARGET_PART}

    echo "========== PASS ${i}: Resizing filesystem..."
    RESIZE_RET=$(resize2fs -M -p ${TARGET_PART} 2>&1)

    DONE_MSG=$(echo "${RESIZE_RET}" | grep "Nothing to do!")
    if [[ -n "${DONE_MSG}" ]]; then
        break;
    fi
done

# Calculate min size of filesystem in GB
MIN_SIZE=$(echo "scale=3; $(echo "${RESIZE_RET}" | grep "blocks long" | grep -Eo " [0-9]+ ") * 4 / 1024 / 1024" | bc)
echo "Shrunk filesystem partition to ${MIN_SIZE} GB"

# Shrink partition itself
# Calculate target partition size in GB (add ~200MB to min size and rounds to nearest .1)
PART_SIZE=$(printf "%.1f\n" $(echo "${MIN_SIZE} + 0.2" | bc))
PART_NUM=$(echo "$TARGET_PART" | sed "s#${NBD_DEV}p##g")

# NOTE: The below is for fdisk v2.31.1
#       This process also assumes the filesystem partition is the last partition on the disk's layout
(
echo p              # Print original partition table
echo d              # Delete partition
echo ${PART_NUM}    # Specify partition to delete
echo n              # Add a new partition
echo ${PART_NUM}    # Specify partition # to create at
echo                # Accept default starting sector
echo +${PART_SIZE}G # Last sector (Accept default: varies)
echo p              # Print new partition table
echo w              # Write changes
) | fdisk ${NBD_DEV}

FDISK_L=$(fdisk -l /dev/nbd6)
SECTOR_SIZE=$(echo "$FDISK_L" | grep "Sector size" | grep -Eo " [0-9]+ " | tail -n1)
END_SECTOR=$(echo "$FDISK_L" | grep "Linux filesystem" | awk '{print $3}')

# Disconnect NBD from image
qemu-nbd -d ${NBD_DEV}

# Calculate size of disk at end of sector
# Target disk size is 200MB beyond end of sector (in bytes)
DISK_SIZE=$(echo "scale=3; ${END_SECTOR} * ${SECTOR_SIZE} + 200 * 1024 * 1024" | bc)

# Get basename of image file w/o the extension
BASENAME=$(echo ${TARGET_IMG} | cut -d '.' -f 1)

echo "Converting image to raw disk format..."
qemu-img convert ${TARGET_IMG} -O raw ${BASENAME}.raw

echo "Shrinking virtual disk size..."
qemu-img resize -f raw --shrink ${BASENAME}.raw ${DISK_SIZE}
qemu-img info ${BASENAME}.raw

echo "In case of GPT, fixing secondary headers..."
(
echo v  # Verify disk (it will report errors)
echo w  # Write changes to disk (will create new secondary header at end of disk)
echo Y  # Confirm write
) | gdisk ${BASENAME}.raw

echo "Converting and compressing raw image to new image: ${BASENAME}-new.qcow2"
qemu-img convert -c ${BASENAME}.raw -O qcow2 ${BASENAME}-new.qcow2

echo "Cleaning up..."
rm ${BASENAME}.raw
rm -rf ${TMP_MOUNT}


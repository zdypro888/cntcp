#!/bin/bash

# Variables
KERNEL_TAR="/usr/src/linux-source-5.15.0/linux-source-5.15.0.tar.bz2"
DRIVER_NAME="cntcp.ko"
IMG_NAME="rootfs.img"
KERNEL_DIR="linux-source-5.15.0"
ROOTFS_DIR="ubuntu_rootfs"
KERNEL_IMAGE="$KERNEL_DIR/arch/arm64/boot/Image"

# Create rootfs using debootstrap if it doesn't exist
if [ ! -d "$ROOTFS_DIR" ]; then
    debootstrap --arch=arm64 focal $ROOTFS_DIR http://ports.ubuntu.com/
    if [ $? -ne 0 ]; then
        echo "debootstrap failed"
        exit 1
    fi

    # Set root password
    echo "root:password" | chroot $ROOTFS_DIR chpasswd
    if [ $? -ne 0 ]; then
        echo "Setting root password failed"
        exit 1
    fi
fi

# Download and extract the kernel source code if it doesn't exist
if [ ! -f "$KERNEL_TAR" ]; then
    echo "Downloading kernel source failed"
    exit 1
fi
if [ ! -d "$KERNEL_DIR" ]; then
    tar -xjf $KERNEL_TAR
    if [ $? -ne 0 ]; then
        echo "Extracting kernel source failed"
        exit 1
    fi
fi

# Configure and compile the kernel if the kernel image doesn't exist
if [ ! -f "$KERNEL_IMAGE" ]; then
    cd $KERNEL_DIR
    make defconfig
    make menuconfig
    make -j$(nproc)
    if [ $? -ne 0 ]; then
        echo "Building kernel failed"
        exit 1
    fi
    cd ..
fi

# Create an empty img file if it doesn't exist
if [ ! -f "$IMG_NAME" ]; then
    dd if=/dev/zero of=$IMG_NAME bs=1M count=2048
    if [ $? -ne 0 ]; then
        echo "Creating image file failed"
        exit 1
    fi

    # Create an ext4 filesystem
    mkfs.ext4 $IMG_NAME
    if [ $? -ne 0 ]; then
        echo "Creating filesystem failed"
        exit 1
    fi
fi

# Mount the filesystem
mkdir -p mnt
mount -o loop $IMG_NAME mnt
if [ $? -ne 0 ]; then
    echo "Mounting image file failed"
    exit 1
fi

# Copy the root filesystem
cp -a $ROOTFS_DIR/* mnt/
if [ $? -ne 0 ]; then
    echo "Copying rootfs failed"
    exit 1
fi

# Create modules directory and copy driver
MODULES_DIR="mnt/lib/modules/5.15.99/kernel/drivers/"
mkdir -p "$MODULES_DIR"
if [ $? -ne 0 ]; then
    echo "Creating modules directory failed"
    exit 1
fi

cp "$DRIVER_NAME" "$MODULES_DIR"
if [ $? -ne 0 ]; then
    echo "Copying driver failed"
    exit 1
fi

# Add module name to /etc/modules
echo "$DRIVER_NAME" | tee -a mnt/etc/modules

# Unmount the filesystem
umount mnt
if [ $? -ne 0 ]; then
    echo "Unmounting image file failed"
    exit 1
fi
rmdir mnt

# Run QEMU
qemu-system-aarch64 -machine virt -cpu cortex-a57 -kernel $KERNEL_IMAGE -drive if=none,file=$IMG_NAME,format=raw,id=hd0 -device virtio-blk-device,drive=hd0 -append "root=/dev/vda console=ttyAMA0" -s -S -nographic

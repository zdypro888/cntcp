#!/bin/bash

# Variables
KERNEL_URL="https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.10.1.tar.xz"
DRIVER_NAME="cntcp.ko"
IMG_NAME="rootfs.img"
KERNEL_DIR="linux-5.10.1"
ROOTFS_DIR="ubuntu_rootfs"

# Create rootfs using debootstrap
sudo debootstrap --arch=arm64 focal $ROOTFS_DIR http://ports.ubuntu.com/

# Download and extract the kernel source code
wget $KERNEL_URL
tar -xf linux-5.10.1.tar.xz

# Configure and compile the kernel
cd $KERNEL_DIR
make defconfig
make menuconfig
make -j$(nproc)
cd ..

# Create an empty img file
dd if=/dev/zero of=$IMG_NAME bs=1M count=2048

# Create an ext4 filesystem
mkfs.ext4 $IMG_NAME

# Mount the filesystem
mkdir mnt
sudo mount -o loop $IMG_NAME mnt

# Copy the root filesystem and driver
sudo cp -a $ROOTFS_DIR/* mnt/
sudo cp $DRIVER_NAME mnt/

# Unmount the filesystem
sudo umount mnt
rmdir mnt

# Run QEMU
qemu-system-aarch64 -machine virt -cpu cortex-a57 -kernel $KERNEL_DIR/arch/arm64/boot/Image -drive if=none,file=$IMG_NAME,format=raw,id=hd0 -device virtio-blk-device,drive=hd0 -append "root=/dev/vda console=ttyAMA0" -s -S


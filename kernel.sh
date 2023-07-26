#!/bin/bash

# Variables
HOME="/root"
DRIVER_NAME="cntcp.ko"
ROOTFS_IMAGE="rootfs.img"
ROOTFS_DIR="/root/ubuntu_rootfs"
KERNEL_DIR="/root/linux-source-5.15.0"
KERNEL_IMAGE="$KERNEL_DIR/arch/arm64/boot/Image"

echo "root:lingling" | chroot $ROOTFS_DIR chpasswd

# Create an empty img file if it doesn't exist
if [ ! -f "$ROOTFS_IMAGE" ]; then
    dd if=/dev/zero of=$ROOTFS_IMAGE bs=1M count=2048
    if [ $? -ne 0 ]; then
        echo "Creating image file failed"
        exit 1
    fi

    # Create an ext4 filesystem
    mkfs.ext4 $ROOTFS_IMAGE
    if [ $? -ne 0 ]; then
        echo "Creating filesystem failed"
        exit 1
    fi
fi

# Mount the filesystem
mkdir -p mnt
mount -o loop $ROOTFS_IMAGE mnt
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

cp $DRIVER_NAME mnt/root/
cp listener/listener mnt/root/
cp -a /etc/netplan/* mnt//etc/netplan/

echo "#!/bin/bash
ip link set eth0 up
dhclient eth0
/sbin/insmod /root/$DRIVER_NAME
" | tee mnt/etc/rc.local > /dev/null

chroot mnt chmod +x /etc/rc.local

echo "[Unit]
Description=Load rc.local
After=network-online.target
Wants=network-online.target

[Service]
Type=oneshot
ExecStart=/etc/rc.local

[Install]
WantedBy=multi-user.target" | tee mnt/etc/systemd/system/rc-local.service > /dev/null

# Enable the service unit
chroot mnt systemctl enable rc-local.service

# Unmount the filesystem
umount mnt
if [ $? -ne 0 ]; then
    echo "Unmounting image file failed"
    exit 1
fi
rmdir mnt

# Run QEMU
qemu-system-aarch64 -machine virt -cpu cortex-a57 -kernel $KERNEL_IMAGE -drive if=none,file=$ROOTFS_IMAGE,format=raw,id=hd0 -device virtio-blk-device,drive=hd0 -append "root=/dev/vda rw console=ttyAMA0"  -nographic -netdev user,id=net0,hostfwd=tcp::8080-:80 -device virtio-net-device,netdev=net0 -s

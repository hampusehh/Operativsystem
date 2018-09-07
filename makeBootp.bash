#!/bin/bash
rm -f bootp.iso
truncate -s 7M bootp.iso
mkfs.ext2 -E root_owner=$(id -u):$(id -g) -U 12345678-1234-1234-1234-123456789abc bootp.iso
if [ $(whoami) = root ]; then
  Folder=bootploop
  mkdir -p bootploop
  mount -o loop bootp.iso bootploop/
else
  sleep 5
  Loop=$(udisksctl loop-setup -f bootp.iso | grep -o '/dev/loop[0-9]\+')
  Folder=$(udisksctl info -b $Loop | grep MountPoints | awk '{print $2}')
fi
mkdir -p $Folder/grub
cat >$Folder/grub/grub.cfg <<EOF
set timeout=1
menuentry "Kernel Image" {
  multiboot (hd0,msdos1)/kernel.bin
  boot
}
EOF
cp kernel.bin $Folder/
find $Folder -type f -exec ls -l \{\} \;

if [ $(whoami) = root ]; then
  umount bootploop
else
  udisksctl unmount -b $Loop
  #udisksctl loop-delete -b $Loop
fi

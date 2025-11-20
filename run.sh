#!/bin/bash

mkdir -p ~/efi-vars
cp /usr/share/edk2/ovmf/OVMF_VARS.fd ~/efi-vars/OVMF_VARS.fd
chmod u+rw ~/efi-vars/OVMF_VARS.fd

qemu-system-x86_64 \
  -cpu qemu64 \
  -drive if=pflash,format=raw,unit=0,file=/usr/share/edk2/ovmf/OVMF_CODE.fd,readonly=on \
  -drive if=pflash,format=raw,unit=1,file=$HOME/efi-vars/OVMF_VARS.fd \
  -drive format=raw,file=efi.img \
  -net none

#qemu-system-x86_64 -cpu qemu64 -bios /usr/share/edk2/ovmf/OVMF_CODE.fd -drive file=efi.img,if=ide



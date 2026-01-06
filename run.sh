#!/bin/bash

if [ "$1" == "debug" ]; then
  echo "DEBUG MODE IS TURNED ON"
  qemu-system-x86_64 \
  -cpu qemu64,rdrand=on \
  -drive if=pflash,format=raw,unit=0,file=/usr/share/edk2/ovmf/OVMF_CODE.fd,readonly=on \
  -drive format=raw,file=efi.img \
  -net none \
  -serial mon:stdio \
  -s -S
else
  qemu-system-x86_64 \
  -cpu qemu64,rdrand=on \
  -drive if=pflash,format=raw,unit=0,file=/usr/share/edk2/ovmf/OVMF_CODE.fd,readonly=on \
  -drive format=raw,file=efi.img \
  -net none
fi

# -drive if=pflash,format=raw,unit=1,file=$HOME/efi-vars/OVMF_VARS.fd \
# mkdir -p ~/efi-vars
# cp /usr/share/edk2/ovmf/OVMF_VARS.fd ~/efi-vars/OVMF_VARS.fd
# chmod u+rw ~/efi-vars/OVMF_VARS.fd





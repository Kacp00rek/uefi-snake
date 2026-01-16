#!/bin/bash

cd "$(dirname "$0")/.."

OVMF_PATH="/usr/share/edk2/ovmf/OVMF_CODE.fd"

if [ "$1" == "debug" ]; then
  echo "DEBUG MODE IS TURNED ON"
  qemu-system-x86_64 \
  -cpu qemu64,rdrand=on \
  -drive if=pflash,format=raw,unit=0,file=$OVMF_PATH,readonly=on \
  -drive format=raw,file=efi.img \
  -net none \
  -serial mon:stdio \
  -s -S
else
  qemu-system-x86_64 \
  -cpu qemu64,rdrand=on \
  -drive if=pflash,format=raw,unit=0,file=$OVMF_PATH,readonly=on \
  -drive format=raw,file=efi.img \
  -net none
fi





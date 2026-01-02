#!/bin/bash

EFI_DIR="$HOME/gnu-efi"

# Compile main.c -> main.o
gcc -I"$EFI_DIR/inc" -fpic -ffreestanding -fno-stack-protector -fno-stack-check \
    -fshort-wchar -mno-red-zone -maccumulate-outgoing-args -ggdb \
    -c main.c -o main.o 

# Link main.o -> main.so
ld -shared -Bsymbolic \
   -L"$EFI_DIR/x86_64/lib" -L"$EFI_DIR/x86_64/gnuefi" \
   -T"$EFI_DIR/gnuefi/elf_x86_64_efi.lds" \
   "$EFI_DIR/x86_64/gnuefi/crt0-efi-x86_64.o" main.o \
   -o main.so -lgnuefi -lefi

# Extract debug symbols
objcopy --only-keep-debug main.so main.efi.debug

# Convert main.so -> main.efi
objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic -j .dynsym \
        -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc \
        --target efi-app-x86_64 --subsystem=10 main.so main.efi

# Initialize disk image if it doesn't exist
if [ ! -f efi.img ]; then
    # Create a 40MB empty disk image
    dd if=/dev/zero of=efi.img bs=512 count=93750
    parted efi.img -s -a minimal mklabel gpt
    parted efi.img -s -a minimal mkpart EFI FAT16 2048s 93716s
    parted efi.img -s -a minimal toggle 1 boot
    
    dd if=/dev/zero of=/tmp/part.img bs=512 count=91669
    mformat -i /tmp/part.img -h 32 -t 32 -n 64 -c 1
    
    dd if=/tmp/part.img of=efi.img bs=512 count=91669 seek=2048 conv=notrunc
fi

dd if=efi.img of=/tmp/part.img bs=512 skip=2048 count=91669 status=none
mkdir -p EFI/BOOT && cp main.efi EFI/BOOT/BOOTX64.EFI
mcopy -o -i /tmp/part.img -s EFI ::
dd if=/tmp/part.img of=efi.img bs=512 count=91669 seek=2048 conv=notrunc status=none


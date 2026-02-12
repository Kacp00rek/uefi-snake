# UEFI Application Snake
<img width="1100" height="443" alt="snake" src="https://github.com/user-attachments/assets/e46a1dc0-34a0-45d3-9151-1d2d6a874120" />

A low-level, bare-metal Snake game written in C for the UEFI environment. This project demonstrates direct interaction with UEFI protocols without an underlying operating system.

## Project Structure
- `src/`: Contains the C source code (`main.c`).
- `scripts/`: Shell scripts for automated building and execution.
- `docs/`: Contains a detailed report about the project.

## Key Features
- **File System**: Hall of Fame management using `SimpleFileSystemProtocol`.
- **Graphics**: Uses the `EFI_GRAPHICS_OUTPUT_PROTOCOL` (GOP) for pixel manipulation.
- **Dynamic Memory**: Custom vector implementation with `AllocatePool` and `FreePool`.
- **Input**: Handles keyboard events via `WaitForKey` and `ReadKeyStroke`.

<img width="413" height="291" alt="menu" src="https://github.com/user-attachments/assets/0542b483-c06c-416b-9f35-2954e1ed3363" />
<img width="200" height="291" alt="hall" src="https://github.com/user-attachments/assets/547656ad-1028-4d6f-88a3-06b99cfa5a01" />

## Requirements
- `gcc`, `binutils` (objcopy, ld)
- `gnu-efi` library
- `mtools`, `parted` (disk image manipulation)
- `qemu-system-x86_64` and `OVMF` firmware
- `gdb` for debugging if needed

## Build & Run

### 1. Compile the project
This generates the `.efi` application and a 46MB GPT-partitioned `efi.img` with a FAT16 ESP.
```bash
chmod +x scripts/compile.sh
scripts/compile.sh
```
Note: This script assumes your gnu-efi library is located in your home folder. If not, update the following line located in the beginning of scripts/compile.sh:
```bash
EFI_DIR="YOUR/EFI/PATH"
```

### 2. Run the project
```bash
chmod +x scripts/run.sh
scripts/run.sh
```
Note: This script assumes the OVMF firmware is located at:
`/usr/share/edk2/ovmf/OVMF_CODE.fd`. If QEMU fails to start, locate the `OVMF_CODE.fd` file on your system and update the path in the beginning of `scripts/run.sh`:
```bash
OVMF_PATH="YOUR/OVMF/PATH"
```

## Debugging
To debug, run the project with the debug flag:
```bash
./scripts/run.sh debug
```
The CPU will freeze at the first instruction, waiting for a debugger connection.

In a **separate** terminal, run the following commands:
```bash
gdb main.efi.debug
target remote localhost:1234
watch *(unsigned long long*)0x10000 == 0xDEADBEEF
continue
set $base = *(unsigned long long*)0x10008
add-symbol-file main.efi.debug -o $base
layout split
```
After synchronization, you can use standard GDB commands like stepi, next, or break.


## Report
Read the full report about the project [here](./docs/report.pdf).

## Resources
This project was heavily inspired by the [OSDev Wiki](https://wiki.osdev.org/UEFI), which is a great resource for anyone interested in UEFI development.



**Author:** Kacper Grzelakowski  

**GitHub:** [github.com/Kacp00rek](https://github.com/Kacp00rek)

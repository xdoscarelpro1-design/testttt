# ────────────────────────────────────────────────────────────────
#  PHANTOM EFI Bootkit — Build System
# ────────────────────────────────────────────────────────────────
#
#  Prerequisites:
#    - gnu-efi development package (apt install gnu-efi)
#    - GCC cross-compiler for EFI (or native x86_64 GCC on Linux)
#    - x86_64-w64-mingw32-gcc (for kernel payload and usermode comm)
#
#  Targets:
#    make all        — build everything
#    make efi        — build the EFI bootkit only
#    make payload    — build the kernel payload only
#    make comm       — build the usermode communicator (Windows .exe)
#    make usb        — create a bootable USB image
#    make clean      — clean all build artifacts
#
# ────────────────────────────────────────────────────────────────

# ─── toolchain ───
CC          := gcc
MINGW_CC    := x86_64-w64-mingw32-gcc
OBJCOPY     := objcopy
LD          := ld

# ─── gnu-efi paths ───
EFI_INC     := /usr/include/efi
EFI_INC_X64 := /usr/include/efi/x86_64
EFI_LIB     := /usr/lib
EFI_CRT     := /usr/lib/crt0-efi-x86_64.o
EFI_LDS     := /usr/lib/elf_x86_64_efi.lds

# ─── directories ───
SRC_EFI     := src/efi
SRC_KERNEL  := src/kernel
SRC_CHEAT   := src/cheat
INC         := include
BUILD       := build
BUILD_EFI   := $(BUILD)/efi
BUILD_KERN  := $(BUILD)/kernel
BUILD_COMM  := $(BUILD)/comm
BUILD_USB   := $(BUILD)/usb

# ─── flags ───
CFLAGS_COMMON := -Wall -Wextra -O2 -I$(INC)

# EFI flags (position-independent, no standard libs)
CFLAGS_EFI  := $(CFLAGS_COMMON) \
               -I$(EFI_INC) -I$(EFI_INC_X64) \
               -fpic -ffreestanding -fno-stack-protector \
               -fno-stack-check -fshort-wchar -mno-red-zone \
               -maccumulate-outgoing-args \
               -DEFI_FUNCTION_WRAPPER \
               -DGNU_EFI_USE_MS_ABI

LDFLAGS_EFI := -nostdlib -znocombreloc -T $(EFI_LDS) \
               -shared -Bsymbolic \
               -L$(EFI_LIB) $(EFI_CRT)

LIBS_EFI    := -lefi -lgnuefi

# Kernel payload flags (position-independent, freestanding)
CFLAGS_KERN := -ffreestanding -fno-stack-protector \
               -fPIC -mno-red-zone -nostdlib \
               -fno-exceptions -fno-asynchronous-unwind-tables \
               -O2 -I$(INC)

# Usermode comm flags (Windows target)
CFLAGS_COMM := -O2 -I$(INC) -DUNICODE -D_UNICODE

# ─── sources ───
EFI_SRCS    := $(SRC_EFI)/main.c \
               $(SRC_EFI)/hooks.c \
               $(SRC_EFI)/pe.c \
               $(SRC_EFI)/patterns.c \
               $(SRC_EFI)/payload.c

KERN_SRCS   := $(SRC_KERNEL)/entry.c \
               $(SRC_KERNEL)/bypass.c

COMM_SRCS   := $(SRC_CHEAT)/fivem_comm.c

# ─── objects ───
EFI_OBJS    := $(patsubst $(SRC_EFI)/%.c,$(BUILD_EFI)/%.o,$(EFI_SRCS))
KERN_OBJS   := $(patsubst $(SRC_KERNEL)/%.c,$(BUILD_KERN)/%.o,$(KERN_SRCS))

# ─── outputs ───
EFI_SO      := $(BUILD_EFI)/bootx64.so
EFI_BIN     := $(BUILD_EFI)/bootx64.efi
KERN_BIN    := $(BUILD_KERN)/kernel_payload.bin
KERN_BLOB   := $(INC)/kernel_payload_blob.h
COMM_EXE    := $(BUILD_COMM)/phantom_comm.exe
USB_IMG     := $(BUILD_USB)/phantom_usb.img

# ════════════════════════════════════════════════════════
#  Targets
# ════════════════════════════════════════════════════════

.PHONY: all efi payload comm usb clean dirs

all: dirs payload efi comm

dirs:
	@mkdir -p $(BUILD_EFI) $(BUILD_KERN) $(BUILD_COMM) $(BUILD_USB)

# ─── kernel payload (built first, embedded in EFI) ───
payload: dirs $(KERN_BIN)
	@echo "[+] Kernel payload built: $(KERN_BIN)"
	@echo "[+] Payload blob generated: $(KERN_BLOB)"

$(BUILD_KERN)/%.o: $(SRC_KERNEL)/%.c
	$(MINGW_CC) $(CFLAGS_KERN) -c $< -o $@

$(BUILD_KERN)/kernel_payload.elf: $(KERN_OBJS)
	$(MINGW_CC) -nostdlib -fPIC -shared \
		-Wl,--entry,PayloadEntry \
		-Wl,-e,PayloadEntry \
		-o $@ $(KERN_OBJS)

$(KERN_BIN): $(BUILD_KERN)/kernel_payload.elf
	$(OBJCOPY) -O binary -j .text $< $@
	@echo "[*] Generating payload blob header..."
	@python3 -c "\
	import sys; \
	data = open('$(KERN_BIN)', 'rb').read(); \
	f = open('$(KERN_BLOB)', 'w'); \
	f.write('#ifndef _KERNEL_PAYLOAD_BLOB_H_\n'); \
	f.write('#define _KERNEL_PAYLOAD_BLOB_H_\n\n'); \
	f.write('static const unsigned char gKernelPayloadBlob[] = {\n'); \
	for i in range(0, len(data), 16): \
	    chunk = data[i:i+16]; \
	    f.write('    ' + ', '.join('0x%02x' % b for b in chunk) + ',\n'); \
	f.write('};\n\n'); \
	f.write('#endif\n'); \
	f.close(); \
	print('[+] Blob: %d bytes' % len(data))"

# ─── EFI bootkit ───
efi: dirs $(EFI_BIN)
	@echo "[+] EFI bootkit built: $(EFI_BIN)"

$(BUILD_EFI)/%.o: $(SRC_EFI)/%.c $(KERN_BLOB)
	$(CC) $(CFLAGS_EFI) -c $< -o $@

$(EFI_SO): $(EFI_OBJS)
	$(LD) $(LDFLAGS_EFI) $(EFI_OBJS) -o $@ $(LIBS_EFI)

$(EFI_BIN): $(EFI_SO)
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic \
		-j .dynsym -j .rel -j .rela -j .reloc \
		--target=efi-app-x86_64 $< $@

# ─── usermode communicator ───
comm: dirs $(COMM_EXE)
	@echo "[+] Communicator built: $(COMM_EXE)"

$(COMM_EXE): $(COMM_SRCS)
	$(MINGW_CC) $(CFLAGS_COMM) $< -o $@ \
		-lntdll -lkernel32 -luser32

# ─── USB image creation ───
usb: all
	@echo "[*] Creating bootable USB image..."
	@mkdir -p $(BUILD_USB)
	dd if=/dev/zero of=$(USB_IMG) bs=1M count=64 2>/dev/null
	mkfs.vfat -F 32 $(USB_IMG) >/dev/null
	mmd -i $(USB_IMG) ::EFI
	mmd -i $(USB_IMG) ::EFI/BOOT
	mcopy -i $(USB_IMG) $(EFI_BIN) ::EFI/BOOT/bootx64.efi
	@echo "[+] USB image created: $(USB_IMG)"
	@echo "[*] Flash to USB with: dd if=$(USB_IMG) of=/dev/sdX bs=4M"

# ─── clean ───
clean:
	rm -rf $(BUILD)
	@echo "[+] Build artifacts cleaned"

# ─── help ───
help:
	@echo "PHANTOM EFI Bootkit — Build Targets"
	@echo ""
	@echo "  make all      — Build everything (payload → EFI → comm)"
	@echo "  make efi      — Build EFI bootkit only"
	@echo "  make payload  — Build kernel payload only"
	@echo "  make comm     — Build usermode communicator"
	@echo "  make usb      — Create bootable USB image"
	@echo "  make clean    — Remove build artifacts"
	@echo ""
	@echo "Prerequisites:"
	@echo "  apt install gnu-efi gcc-mingw-w64-x86-64 mtools python3"

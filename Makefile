# ────────────────────────────────────────────────────────────────
#  PHANTOM EFI Bootkit — Build System (mingw-direct PE)
# ────────────────────────────────────────────────────────────────
#
#  Build chain (order matters):
#    1. Cheat DLL      — x86_64 Windows DLL (MSVC on Windows)
#    2. Cheat blob     — DLL binary → C byte array header
#    3. Kernel payload — PIC kernel code + embedded cheat blob
#    4. Payload blob   — kernel binary → C byte array header
#    5. EFI bootkit    — UEFI PE32+ application embedding the payload
#
#  No gnu-efi dependency. Compiled directly to PE with mingw
#  using --subsystem,10 (EFI application). Tested on real hardware.
#
#  Prerequisites:
#    - x86_64-w64-mingw32-gcc
#    - objcopy (for kernel payload binary extraction)
#    - python3 (for blob generation)
#    - mtools (for USB image)
#
#  Targets:
#    make all        — build everything (cheat → kernel → EFI)
#    make efi        — build the EFI bootkit only
#    make payload    — build the kernel payload only
#    make cheatdll   — build the cheat DLL only
#    make usb        — create a bootable USB image
#    make clean      — clean all build artifacts
#
# ────────────────────────────────────────────────────────────────

# ─── toolchain ───
MINGW_CC    := x86_64-w64-mingw32-gcc
MINGW_CXX   := x86_64-w64-mingw32-g++
OBJCOPY     := objcopy

# ─── directories ───
SRC_EFI     := src/efi
SRC_KERNEL  := src/kernel
SRC_CHEAT   := src/cheat
INC         := include
BUILD       := build
BUILD_EFI   := $(BUILD)/efi
BUILD_KERN  := $(BUILD)/kernel
BUILD_CHEAT := $(BUILD)/cheat
BUILD_USB   := $(BUILD)/usb

# ─── EFI flags (mingw-direct PE, no gnu-efi) ───
CFLAGS_EFI  := -Wall -Wextra -O2 -I$(INC) \
               -ffreestanding -fno-stack-protector \
               -fno-stack-check -fshort-wchar -mno-red-zone \
               -maccumulate-outgoing-args -fno-builtin

# ─── Kernel payload flags (position-independent, freestanding) ───
CFLAGS_KERN := -ffreestanding -fno-stack-protector \
               -fPIC -mno-red-zone -nostdlib \
               -fno-exceptions -fno-asynchronous-unwind-tables \
               -O2 -I$(INC)

# ─── Cheat DLL flags (Windows x86_64 usermode) ───
CFLAGS_CHEAT := -O2 -I$(INC) -I$(SRC_CHEAT) \
                -DUNICODE -D_UNICODE \
                -std=c++17 -fno-rtti -fno-exceptions \
                -mwindows

# ─── sources ───
EFI_SRCS    := $(SRC_EFI)/main.c \
               $(SRC_EFI)/hooks.c \
               $(SRC_EFI)/pe.c \
               $(SRC_EFI)/patterns.c \
               $(SRC_EFI)/payload.c \
               $(SRC_EFI)/efi_util.c

KERN_SRCS   := $(SRC_KERNEL)/entry.c \
               $(SRC_KERNEL)/bypass.c \
               $(SRC_KERNEL)/monitor.c \
               $(SRC_KERNEL)/injector.c

CHEAT_SRCS  := $(wildcard $(SRC_CHEAT)/*.cpp) \
               $(wildcard $(SRC_CHEAT)/Cheat/*.cpp) \
               $(wildcard $(SRC_CHEAT)/Cheat/Features/**/*.cpp) \
               $(wildcard $(SRC_CHEAT)/FrameWork/**/*.cpp)

# ─── objects ───
EFI_OBJS    := $(patsubst $(SRC_EFI)/%.c,$(BUILD_EFI)/%.o,$(EFI_SRCS))
KERN_OBJS   := $(patsubst $(SRC_KERNEL)/%.c,$(BUILD_KERN)/%.o,$(KERN_SRCS))

# ─── outputs ───
EFI_BIN     := $(BUILD_EFI)/bootx64.efi
KERN_BIN    := $(BUILD_KERN)/kernel_payload.bin
KERN_BLOB   := $(INC)/kernel_payload_blob.h
CHEAT_DLL   := $(BUILD_CHEAT)/phantom_cheat.dll
CHEAT_BLOB  := $(INC)/cheat_payload_blob.h
USB_IMG     := $(BUILD_USB)/phantom_usb.img

# ════════════════════════════════════════════════════════
#  Targets
# ════════════════════════════════════════════════════════

.PHONY: all efi payload cheatdll usb clean dirs help

all: dirs cheatdll payload efi
	@echo ""
	@echo "══════════════════════════════════════════════"
	@echo "  PHANTOM build complete (mingw-direct PE)"
	@echo "  EFI bootkit:  $(EFI_BIN)"
	@echo "  Cheat DLL:    $(CHEAT_DLL) (embedded)"
	@echo "  Kernel:       $(KERN_BIN) (embedded)"
	@echo "══════════════════════════════════════════════"

dirs:
	@mkdir -p $(BUILD_EFI) $(BUILD_KERN) $(BUILD_CHEAT) $(BUILD_USB)

# ─── cheat DLL (built first, embedded in kernel payload) ───
cheatdll: dirs $(CHEAT_BLOB)
	@echo "[+] Cheat DLL built: $(CHEAT_DLL)"
	@echo "[+] Cheat blob generated: $(CHEAT_BLOB)"

$(CHEAT_DLL): | dirs
	@if [ -f $@ ]; then \
		echo "[*] Cheat DLL found: $@ (pre-built by build.bat/MSVC)"; \
	else \
		echo "[ERROR] Cheat DLL not found: $@"; \
		echo "        Build it first on Windows with: build.bat"; \
		exit 1; \
	fi

$(CHEAT_BLOB): $(CHEAT_DLL)
	@echo "[*] Converting cheat DLL to embedded blob..."
	@python3 -c "data=open('$(CHEAT_DLL)','rb').read();f=open('$(CHEAT_BLOB)','w');f.write('#ifndef _CHEAT_PAYLOAD_BLOB_H_\n#define _CHEAT_PAYLOAD_BLOB_H_\n\n__attribute__((used, section(\".rdata\"))) const unsigned char gCheatPayloadBlob[] = {\n');[f.write('    '+', '.join('0x%02x'%b for b in data[i:i+16])+',\n') for i in range(0,len(data),16)];f.write('};\n\nstatic const unsigned int gCheatPayloadSize = sizeof(gCheatPayloadBlob);\n\n#endif\n');f.close();print('[+] Cheat blob: %d bytes embedded'%len(data))"

# ─── kernel payload (depends on cheat blob) ───
payload: dirs cheatdll $(KERN_BIN)
	@echo "[+] Kernel payload built: $(KERN_BIN)"
	@echo "[+] Payload blob generated: $(KERN_BLOB)"

$(BUILD_KERN)/%.o: $(SRC_KERNEL)/%.c $(CHEAT_BLOB)
	$(MINGW_CC) $(CFLAGS_KERN) -c $< -o $@

$(BUILD_KERN)/kernel_payload.elf: $(KERN_OBJS)
	$(MINGW_CC) -nostdlib -fPIC -shared \
		-Wl,--entry,PayloadEntry \
		-Wl,-e,PayloadEntry \
		-Wl,-T,$(SRC_KERNEL)/payload.ld \
		-o $@ $(KERN_OBJS)

$(KERN_BIN): $(BUILD_KERN)/kernel_payload.elf
	$(OBJCOPY) -O binary $< $@
	@echo "[*] Generating payload blob header..."
	@python3 -c "data=open('$(KERN_BIN)','rb').read();f=open('$(KERN_BLOB)','w');f.write('#ifndef _KERNEL_PAYLOAD_BLOB_H_\n#define _KERNEL_PAYLOAD_BLOB_H_\n\nstatic const unsigned char gKernelPayloadBlob[] = {\n');[f.write('    '+', '.join('0x%02x'%b for b in data[i:i+16])+',\n') for i in range(0,len(data),16)];f.write('};\n\n#endif\n');f.close();print('[+] Payload blob: %d bytes'%len(data))"

# ─── EFI bootkit (mingw-direct PE32+, no gnu-efi) ───
efi: dirs payload $(EFI_BIN)
	@echo "[+] EFI bootkit built: $(EFI_BIN)"

$(BUILD_EFI)/%.o: $(SRC_EFI)/%.c $(KERN_BLOB)
	$(MINGW_CC) $(CFLAGS_EFI) -c $< -o $@

$(EFI_BIN): $(EFI_OBJS)
	$(MINGW_CC) -nostdlib -e efi_main \
		-Wl,--subsystem,10 \
		-Wl,--dynamicbase \
		-Wl,--nxcompat \
		-o $@ $(EFI_OBJS)

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
	@echo "PHANTOM EFI Bootkit — Build Targets (mingw-direct PE)"
	@echo ""
	@echo "  make all       — Full build: cheat DLL → kernel → EFI"
	@echo "  make cheatdll  — Build cheat DLL + generate blob header"
	@echo "  make payload   — Build kernel payload (requires cheat blob)"
	@echo "  make efi       — Build EFI bootkit (requires kernel blob)"
	@echo "  make usb       — Create bootable USB image"
	@echo "  make clean     — Remove build artifacts"
	@echo ""
	@echo "Build chain:"
	@echo "  cheat .cpp → DLL → C blob → kernel .c → binary → C blob → EFI"
	@echo ""
	@echo "Output: Single bootx64.efi containing everything."
	@echo "        Nothing touches disk. Forensically invisible."
	@echo ""
	@echo "Prerequisites:"
	@echo "  apt install gcc-mingw-w64-x86-64 mtools python3"

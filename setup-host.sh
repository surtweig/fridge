#!/usr/bin/env bash
# One-time host setup for the Spartan-6 toolchain.
# Run with:  sudo ./setup-host.sh
#
# 1. Relocates Docker's data-root to /mnt/data/docker (634 GB free vs 72 GB on /)
# 2. Installs openFPGALoader (board programming)
# 3. Installs a udev rule for the Digilent onboard USB-JTAG (1443:0007)

set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
    echo "error: run this script with sudo" >&2
    exit 1
fi

NEW_ROOT=/mnt/data/docker
OLD_ROOT=/var/lib/docker

echo "==> [1/3] Docker data-root -> $NEW_ROOT"
mkdir -p "$NEW_ROOT"

if [ -f /etc/docker/daemon.json ] && grep -q "$NEW_ROOT" /etc/docker/daemon.json; then
    echo "    data-root already configured, skipping"
else
    if [ -d "$OLD_ROOT" ] && [ -z "$(ls -A "$NEW_ROOT" 2>/dev/null)" ]; then
        echo "    copying existing Docker data from $OLD_ROOT (keeps current images)..."
        if command -v rsync >/dev/null 2>&1; then
            rsync -a "$OLD_ROOT"/ "$NEW_ROOT"/
        else
            cp -a "$OLD_ROOT"/. "$NEW_ROOT"/
        fi
    fi

    mkdir -p /etc/docker
    printf '{\n  "data-root": "%s"\n}\n' "$NEW_ROOT" > /etc/docker/daemon.json
    systemctl restart docker
    echo "    done (old data left at $OLD_ROOT; reclaim it later with rm -rf once satisfied)"
fi

echo "==> [2/3] openFPGALoader"
pacman -S --noconfirm --needed openfpgaloader

echo "==> [3/3] udev rule for Digilent JTAG"
cat > /etc/udev/rules.d/99-digilent-jtag.rules <<'EOF'
# Digilent onboard USB-JTAG (Atlys et al., FTDI FT2232H with Digilent VID/PID)
SUBSYSTEM=="usb", ATTR{idVendor}=="1443", ATTR{idProduct}=="0007", MODE="0666", TAG+="uaccess"
# FTDI-based Digilent cables (JTAG-HS2 etc.) and generic FT2232/FT4232
SUBSYSTEM=="usb", ATTR{idVendor}=="0403", MODE="0666", TAG+="uaccess"
EOF
udevadm control --reload-rules
udevadm trigger

echo
echo "All done. Verify with:"
echo "  docker info | grep 'Docker Root Dir'"
echo "  openFPGALoader --cable digilent --vid 0x1443 --pid 0x0007 --detect"

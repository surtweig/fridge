#!/usr/bin/env bash
# Second-stage host setup for the Spartan-6 toolchain.
# Run with:  sudo ./setup-system.sh
#
# 1. Relocates system containerd's root to /mnt/data/containerd.
#    (Docker 29 runs against the system containerd; image layers land there,
#    NOT in Docker's data-root, so this is what actually fixes the "no space
#    left on device" during image builds.)
# 2. Installs Digilent Adept (djtgcfg) system-wide from vendor/adept-root.
#    Needed for the Atlys' onboard USB-JTAG (Digilent FX2 protocol, 1443:0007)
#    which openFPGALoader cannot drive.
# 3. Installs docker-buildx (optional: enables BuildKit for leaner builds).

set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
    echo "error: run this script with sudo" >&2
    exit 1
fi

BASE=/mnt/data/Projects/Spartan6Fridge/Spartan6Toolchain
NEW=/mnt/data/containerd
OLD=/var/lib/containerd

echo "==> [1/3] containerd root -> $NEW"
mkdir -p "$NEW"
if grep -qs "$NEW" /etc/containerd/config.toml 2>/dev/null; then
    echo "    already configured, skipping"
else
    echo "    stopping docker + containerd and copying state..."
    systemctl stop docker.socket docker containerd
    if [ -d "$OLD" ] && [ -z "$(ls -A "$NEW" 2>/dev/null)" ]; then
        if command -v rsync >/dev/null 2>&1; then
            rsync -a "$OLD"/ "$NEW"/
        else
            cp -a "$OLD"/. "$NEW"/
        fi
    fi
    mkdir -p /etc/containerd
    printf 'version = 2\nroot = "%s"\n' "$NEW" > /etc/containerd/config.toml
    systemctl start containerd
    systemctl start docker.socket docker
    echo "    done (old data left at $OLD; reclaim later with rm -rf once satisfied)"
fi

echo "==> [2/3] Digilent Adept (djtgcfg) system-wide"
SRC="$BASE/vendor/adept-root"
if [ ! -x "$SRC/usr/bin/djtgcfg" ]; then
    echo "error: $SRC not populated; expected extracted Adept debs there" >&2
    exit 1
fi
install -d -m755 /usr/lib/digilent /usr/share/digilent /usr/local/bin /etc/udev/rules.d /etc/ld.so.conf.d
cp -a "$SRC/usr/lib/digilent/adept" /usr/lib/digilent/
cp -a "$SRC/usr/share/digilent/." /usr/share/digilent/
cp "$SRC/usr/bin/djtgcfg" "$SRC/usr/bin/dadutil" "$SRC/usr/bin/dsumecfg" /usr/local/bin/
cp "$SRC/etc/digilent-adept.conf" /etc/digilent-adept.conf
cp "$SRC/etc/udev/rules.d/52-digilent-usb.rules" /etc/udev/rules.d/
echo /usr/lib/digilent/adept > /etc/ld.so.conf.d/digilent-adept.conf
ldconfig
udevadm control --reload-rules
udevadm trigger

echo "==> [3/3] docker-buildx (optional, enables BuildKit)"
pacman -S --noconfirm --needed docker-buildx || echo "    (skipped)"

echo
echo "All done. Verify with:"
echo "  djtgcfg enum"
echo "  docker info | grep -i containerd"

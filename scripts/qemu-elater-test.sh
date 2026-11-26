#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
#
# Exercise both the report and the platform gates without touching a host ESP.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
efi=${1:-"$root/edk2/Build/Meridian/RELEASE_CLANGDWARF/X64/Elater.efi"}
code=${OVMF_CODE:-/usr/share/edk2/x64/OVMF_CODE.4m.fd}
vars=${OVMF_VARS:-/usr/share/edk2/x64/OVMF_VARS.4m.fd}
work=$(mktemp -d)
pid=
stop_vm() {
    if [[ -n $pid ]]; then
        kill "$pid" 2>/dev/null || true
        wait "$pid" 2>/dev/null || true
        pid=
    fi
}
trap 'stop_vm; rm -rf "$work"' EXIT
for profile in intel mac amd; do
    vendor=GenuineIntel
    maker=Bench
    marker='report written to elater-report.txt'
    case $profile in
        mac) maker='Apple Inc.'; marker='Elater requires a non-Apple Intel PC' ;;
        amd) vendor=AuthenticAMD; marker='Elater requires a non-Apple Intel PC' ;;
    esac
    disk="$work/$profile.img"
    truncate -s 64M "$disk"
    mkfs.vfat "$disk" >/dev/null
    mmd -i "$disk" ::/EFI ::/EFI/BOOT
    mcopy -i "$disk" "$efi" ::/EFI/BOOT/BOOTX64.EFI
    cp "$vars" "$work/vars"
    qemu-system-x86_64 -machine q35,accel=tcg -cpu "max,vendor=$vendor" -m 256 \
        -smbios "type=1,manufacturer=$maker,product=ElaterBench" \
        -drive "if=pflash,format=raw,readonly=on,file=$code" \
        -drive "if=pflash,format=raw,file=$work/vars" \
        -drive "if=none,format=raw,id=drive0,file=$disk" -device nvme,drive=drive0,serial=ELATERTEST \
        -nic none -display none -serial "file:$work/serial" -no-reboot &
    pid=$!
    for ((i=0; i<100; ++i)); do
        grep -aFq "$marker" "$work/serial" 2>/dev/null && break
        sleep 0.2
    done
    stop_vm
    grep -aFq "$marker" "$work/serial" || { echo "$profile: expected message not found" >&2; exit 1; }
    if [[ $profile == intel ]]; then
        mtype -i "$disk" ::/elater-report.txt > "$work/report"
        grep -Fq 'ELATER REPAIR REPORT' "$work/report"
        grep -Eq 'Storage .*QEMU NVMe' "$work/report"
        grep -Eq 'NVMe SMART .*OK .*warning 0x00' "$work/report"
    elif mdir -i "$disk" ::/elater-report.txt >/dev/null 2>&1; then
        echo "$profile: unsupported platform wrote a report" >&2
        exit 1
    fi
    echo "$profile: passed"
done

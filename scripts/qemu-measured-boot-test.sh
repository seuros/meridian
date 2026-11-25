#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Abdelkader Boudih <oss@seuros.com>
#
# Boot Meridian, not the shell, from a disposable ESP. The shell is Meridian's
# measured child; its startup script runs the upstream read-only TPM checks.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
build="$root/edk2/Build"
efi=${MERIDIAN_EFI:-$build/Meridian/DEBUG_CLANGDWARF/X64/Meridian.efi}
app=${TPM_APP_EFI:-$build/SecurityPkg/RELEASE_CLANGDWARF/X64/TpmShellApp.efi}
shell=${SHELL_EFI:-/usr/share/edk2-shell/x64/Shell.efi}
code=${OVMF_CODE:-/usr/share/edk2/x64/OVMF_CODE.4m.fd}
vars=${OVMF_VARS:-/usr/share/edk2/x64/OVMF_VARS.4m.fd}
for file in "$efi" "$app" "$shell" "$code" "$vars"; do
    [[ -r "$file" ]] || { echo "Missing $file (see CONTRIBUTING.md)" >&2; exit 1; }
done
for cmd in qemu-system-x86_64 swtpm sgdisk mformat mcopy iconv sha256sum timeout; do
    command -v "$cmd" >/dev/null || { echo "Missing $cmd" >&2; exit 1; }
done

work=$(mktemp -d /tmp/meridian-measured-boot.XXXXXX)
cleanup() {
    local status=$?
    if [[ -n ${tpid:-} ]]; then
        kill "$tpid" 2>/dev/null || true
        wait "$tpid" 2>/dev/null || true
    fi
    if (( status == 0 )); then rm -rf "$work"; else echo "Test artifacts: $work" >&2; fi
}
trap cleanup EXIT
cd "$work"
mkdir -p esp/EFI/{BOOT,Meridian} tpm
cp "$efi" esp/EFI/BOOT/BOOTX64.EFI
cp "$shell" esp/Shell.efi
cp "$app" esp/TpmShellApp.efi
cat >esp/EFI/BOOT/config.conf <<'CONF'
timeout 1
textonly
serial on
scanfor manual
measured_boot true
default_selection "Measured Shell"
menuentry "Measured Shell" {
    loader \Shell.efi
}
CONF
cp esp/EFI/{BOOT,Meridian}/config.conf
cat >esp/startup.nsh <<'NSH'
@echo -off
fs0:
fs0:\TpmShellApp.efi dump_event_log >a fs0:\dump.txt
fs0:\TpmShellApp.efi replay_event_log >a fs0:\replay.txt
reset -s
NSH

truncate -s 80M disk.img
sgdisk -n 1:2048:+64M -t 1:ef00 disk.img >/dev/null
fat="disk.img@@1048576"
mformat -i "$fat" -T 131072 -F ::
mcopy -i "$fat" -s esp/* ::
cp "$vars" vars.fd
swtpm socket --tpm2 --tpmstate dir=tpm --ctrl type=unixio,path=tpm.sock \
    --terminate >swtpm.log 2>&1 &
tpid=$!
for ((i=0; i<50; i++)); do
    [[ -S tpm.sock ]] && break
    kill -0 "$tpid" && sleep .1
done
[[ -S tpm.sock ]] || { echo "swtpm did not create its socket" >&2; exit 1; }
timeout -k 5 "${BOOT_TIMEOUT:-90}" qemu-system-x86_64 \
    -machine q35,accel=tcg -m 512 -display none -monitor none -serial file:serial.log \
    -nic none -no-reboot -drive "if=pflash,format=raw,readonly=on,file=$code" \
    -drive if=pflash,format=raw,file=vars.fd -drive format=raw,file=disk.img \
    -chardev socket,id=tpmsock,path=tpm.sock -tpmdev emulator,id=tpm0,chardev=tpmsock \
    -device tpm-tis,tpmdev=tpm0

for report in dump replay; do
    mcopy -i "$fat" "::$report.txt" "$report.txt"
    tr -d '\r' <"$report.txt" >"$report.log"
done
# MeasureLoader hashes StrSize(Identifier), including the trailing CHAR16 NUL.
identity=$(printf 'Shell.efi\0' | iconv -f ASCII -t UTF-16LE | sha256sum)
awk -v expected="${identity%% *}" '
    /^Event [0-9]+:/ { active = ($4 == 8 && $5 == "EV_EFI_BOOT_SERVICES_APPLICATION") }
    active && $1 == "SHA256:" { $1 = ""; gsub(/ /, ""); if (tolower($0) == expected) found++ }
    END { exit !found }
' dump.log || { echo "Missing SHA-256 measurement for Shell.efi in PCR 8" >&2; exit 1; }
for pcr in 8 9; do
    grep -Eq "^Event [0-9]+: PCR $pcr EV_EFI_BOOT_SERVICES_APPLICATION " dump.log ||
        { echo "Missing Meridian boot application event in PCR $pcr" >&2; exit 1; }
    # A global PASS (or only skipped banks) doesn't prove either Meridian PCR.
    awk -v want="$pcr:" '
        /^PCR [0-9]+:/ { active = ($2 == want) }
        active && /Result: PASS/ { passed++ }
        active && (/Result: FAIL/ || /read failed/) { failed++ }
        END { exit !(passed && !failed) }
    ' replay.log || { echo "PCR $pcr did not replay against the TPM" >&2; exit 1; }
done
grep -E '^Summary: [1-9][0-9]* PCR bank\(s\) verified, [1-9][0-9]* PASS, 0 FAIL$' replay.log ||
    { echo "Incomplete or failed TPM replay" >&2; exit 1; }
echo "PASS: Meridian loader identity (PCR 8) and image (PCR 9) logged and replayed"

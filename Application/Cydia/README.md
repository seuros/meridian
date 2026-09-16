# Cydia

Pre-OS hardware diagnostic for Intel Macs. Boots from a FAT stick on a machine
that may not POST into any OS, reads the hardware directly, and writes a
plain-language repair report.

Named after *Cydia pomonella*, the codling moth, the worm in the apple.

## Scope

**Apple Intel hardware, 2006-2020.** That is the Intel Mac era, and it bounds
the tool at both ends: before it the machines are PowerPC and never boot this
binary, after it Apple Silicon has no SMC to read and no UEFI to read it from.

Inside that window Cydia takes whichever SMC path the machine offers: Apple's
own SMC EFI protocol when the firmware publishes it, otherwise MMIO, otherwise
the legacy port pair. On non-Apple hardware the SMC-backed checks are skipped
and only the UEFI-layer probes report.

## What it reads

SMC keys carry the parts the UEFI layer cannot see: battery state of charge,
cycle count, design and full capacity and voltage (`BRSC`, `B0CT`, `B0FC`,
`B0DC`, `B0AV`), fan count and CPU proximity temperature (`FNum`, `TC0P`), the
last shutdown cause (`MSSD`, `MSSP`), and the panel sensor.

Everything else comes from what the firmware publishes: SMBIOS for model,
serial, CPU and memory, GOP/UGA plus EDID for display, BlockIo for storage, SNP
for network, TCG2 for the TPM, and UsbIo for enumerated devices.

The result is diffed against a per-model expectation table keyed on the SMBIOS
product name (`MacBookPro`, `MacBookAir`, `iMac`, `Macmini`, `MacPro`, …). That
is what makes an absence meaningful: a missing battery on a MacBook is a fault,
a missing battery on a Mac mini is the Mac mini working correctly.

## Output

Printed to the console and written to `cydia-report.txt` on the volume the
application was loaded from:

```
================ CYDIA REPAIR REPORT ================
Model    : MacBookPro11,1
...
Display .......... DEGRADED  1440x900 EDID present but SMC LCD sensor disconnected
Battery .......... MISSING (expected)
-----------------------------------------------------
VERDICT  : DEFECTIVE -- see MISSING/DEGRADED lines  (1 missing, 1 degraded)
```

| State | Meaning |
| --- | --- |
| `OK` | Present and reporting sane values. |
| `DEGRADED` | Present but wrong: worn battery, framebuffer without EDID, sensor mismatch. |
| `UNKNOWN` | Not observable from the pre-OS environment, so not counted against the machine. |
| `MISSING` | Absent, and the model table says this machine shipped with one. |
| `absent` | Absent, and not expected on this model. |

The cross-check is what earns the tool its place. A panel that reports a valid
EDID while the SMC says its LCD sensor is disconnected is a different repair
from a panel that reports nothing at all, and only reading both sources tells
them apart.

## Safety

Cydia does not write to the subject machine. The only write it performs is the
report on its own boot volume, and it returns `EFI_SUCCESS` regardless of what
it found. The fault count lives in the report, not the exit status.

## Files

- `Cydia.c`: entry point, expectation matching, report formatting and writing.
- `scan.c`: the per-subsystem probes, SMC access, and the model table.
- `cydia.h`: report/component structures and the scan function list.

Adding a model is a row in `gModels` in `scan.c`: an SMBIOS name prefix, a mask
of the parts that model shipped with, and its DIMM slot count. The table is
scanned top to bottom with a prefix compare and the first hit wins, so the
specific entry has to come first: `iMacPro` above `iMac`, `MacBookPro` and
`MacBookAir` above `MacBook`.

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

Board electrical readings are separate from battery voltage. Like FreeBSD's
[`asmc(4)`](https://github.com/freebsd/freebsd-src/blob/9fe57fe3578a3d53d00ba979d13fe3f68f1470fb/sys/dev/asmc/asmc.c#L660-L739),
Cydia discovers voltage (`V…`), current (`I…`) and power (`P…`) keys and uses
each key's type and size to decode it. The console and `cydia-report.txt` list
the raw key names and readings in volts, amps and watts. Power readings can
cover overlapping rails, so there is no calculated total. An unreadable value
is not zero, and neither changes the repair verdict.

Everything else comes from what the firmware publishes: SMBIOS for model,
serial, CPU and memory, GOP/UGA plus EDID for display, BlockIo for storage, SNP
for network, and UsbIo for enumerated devices. The TCG2/TPM check is for
non-Apple firmware only; it is not a test for Apple's T2 chip.
The `Memory` line lists only RAM families used in Intel Macs: DDR2 (including
fully buffered DIMMs), DDR3, LPDDR3, DDR4 and the LPDDR4 family. SMBIOS Type 17
reports DDR3L as DDR3 and LPDDR4X as LPDDR4; Cydia does not guess the variant.
If firmware does not identify a supported type for a populated slot, Cydia
reports it as unverified, not as a fault. Empty slots do not contribute a type.

The result is diffed against a per-model expectation table keyed on the SMBIOS
product name (`MacBookPro`, `MacBookAir`, `iMac`, `Macmini`, `MacPro`, …). That
is what makes an absence meaningful: a missing battery on a MacBook is a fault.
On an iMac (including Pro), Mac mini, Mac Pro or Xserve, the battery check is
`N/A` and Cydia does not query the battery. The Mac model takes precedence over
the firmware chassis label, which can say “Laptop” even on an iMac.

A drive-bay sensor reading of −127°C or −128°C is informational unless the Mac
model expects the sensor and the bay drive identifies itself as an Apple HDD
with a reported rotation rate. An SSD, an unidentified disk or an ambiguous bay
does not turn that reading into a repair fault.

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
| `N/A` | The check does not apply to this Mac model and was skipped. |
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
- `electrical.c`: discovery and decoding of SMC voltage, current and power keys.
- `cydia.h`: report/component structures and the scan function list.

Adding a model is a row in `gModels` in `scan.c`: an SMBIOS name prefix and a
mask of the parts that model shipped with. DIMM slot counts come from the
machine's own SMBIOS records, not a family-wide estimate. The table is
scanned top to bottom with a prefix compare and the first hit wins, so the
specific entry has to come first: `iMacPro` above `iMac`, `MacBookPro` and
`MacBookAir` above `MacBook`.

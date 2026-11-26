# Elater

Elater is the PC counterpart to Cydia. It runs on **non-Apple Intel PCs with
64-bit UEFI** and writes `elater-report.txt` in the root of the volume from
which it was launched. Run `Elater.efi` from the UEFI shell on a writable FAT
stick, or install it as `\EFI\BOOT\BOOTX64.EFI` on a dedicated removable stick.
It returns to its caller after printing the report. Reports contain machine
identifiers; redact the serial number and MAC address before sharing.

| Probe | What it can establish before an OS starts |
| --- | --- |
| CPU and memory | Intel CPU brand, SMBIOS slot population, RAM generation and capacity. |
| Chassis and battery | Firmware enclosure type and portable battery inventory, including design capacity when available. Mini PCs aren't flagged for missing batteries. Inventory is **not** a charge, wear, or live voltage measurement. |
| Storage | Disks visible through UEFI Block I/O; model and capacity when available. NVMe SMART uses a read-only controller health log for temperature, spare, endurance used, and critical warnings. No drive self-test is started. |
| Graphics, network, TPM and USB | Firmware framebuffer and EDID, network interfaces exposed by SNP, TPM presence and active PCR banks, and enumerated USB interfaces. |

`UNKNOWN` means the firmware didn't provide enough evidence, not that a part is
broken. A framebuffer alone doesn't establish that a monitor is plugged in.
Elater doesn't access an Apple SMC or guess at motherboard voltage, current,
fans, or power. It doesn't write to hardware settings or change boot order.

## Testing

After building x64, run `scripts/qemu-elater-test.sh` to check NVMe reporting
under OVMF and verify that Apple and non-Intel profiles don't write reports.

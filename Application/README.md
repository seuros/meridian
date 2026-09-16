# Application/

Standalone UEFI applications. Each one is its own `.efi` with its own entry
point; neither links anything from the Meridian boot manager, and neither is
part of `Meridian.efi`. Put them on a FAT stick and run them from the UEFI
shell, or launch them from the Meridian menu as EFI tools.

| Application | Binary | Purpose |
| --- | --- | --- |
| [Cydia](Cydia/README.md) | `Cydia.efi` | Hardware diagnostic for Intel Macs. Writes a repair report. |
| [DarkPassenger](DarkPassenger/README.md) | `DarkPassenger.efi` | Maps what the vendor firmware fused off, ahead of a coreboot port. |

Both are registered in `[Components]` in `MeridianPkg.dsc` and shipped as loose
release downloads by `scripts/package-release.sh`.

## Adding an application

Applications here are standalone by rule. They do not link Meridian core code,
because they have to run on machines where the boot manager is not what you are
debugging. A new one needs:

- `Application/<Name>/` holding the sources, an `.inf` (`MODULE_TYPE =
  UEFI_APPLICATION`, fresh `FILE_GUID`), and a `README.md` covering what the
  tool does, what hardware it expects, and what it writes.
- An entry in `[Components]` in `MeridianPkg.dsc`.
- A row in the table above, linking that README.
- If it is meant to be downloadable: its base name in `APPLICATIONS` in
  `scripts/package-release.sh` and a row in the download table in the root
  `README.md`.

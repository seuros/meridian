# Contributing

Meridian is firmware code. Keep changes narrow, reviewable, and tied to one
behavioral goal.

## Before You Change Code

Run:

```sh
./scripts/check.sh
```

Install local commit hooks once:

```sh
pre-commit install
```

If you are changing build metadata, also run:

```sh
./scripts/build.sh --workspace /path/to/edk2 --dry-run
```

## Measured-boot smoke test (X64)

`mise run smoke-tpm` boots Meridian under OVMF with a fresh software TPM, then
chain-loads a UEFI shell. It checks that Meridian logged the loader identity in
PCR 8 and the image in PCR 9, and that replay of both matches the TPM. It never
uses the host TPM or changes the host's PCR banks.

Install QEMU, TPM-capable OVMF, `swtpm`, `mtools`, `sgdisk` and an X64 UEFI
shell. Build Meridian and the test-only EDK II app from the pinned tree:

```bash
git submodule update --init --recursive
mise run workspace
mise run build
(
    cd edk2
    source edksetup.sh
    build -p SecurityPkg/SecurityPkg.dsc \
          -m SecurityPkg/Applications/TpmShellApp/TpmShellApp.inf \
          -a X64 -b RELEASE -t CLANGDWARF
)
mise run smoke-tpm
```

The defaults use Arch's `edk2-ovmf` and `edk2-shell` paths. Elsewhere, set
`OVMF_CODE`, `OVMF_VARS` (a matching blank template), and `SHELL_EFI` to
absolute paths. `TPM_APP_EFI` can point at a prebuilt `TpmShellApp.efi` from
the pinned release. Failed runs print the directory containing the disk image,
TPM state and logs.

## Change Rules

- Prefer the EDK2 package files as the source of truth.
- Do not reintroduce inherited subdirectory makefiles. EDK2 package metadata is
  the source of truth for built files.
- Keep imported/vendor code separate from local Meridian behavior.
- Do not mass-reformat unrelated files.
- Preserve original copyright and license provenance.
- Add tests or host-side checks for parsers, path handling, and launch policy
  whenever code can be exercised outside firmware.

## Style

Follow the surrounding C style for touched code until a project-wide formatter
policy is adopted. Small consistency edits in touched functions are fine;
tree-wide whitespace churn is not.

## Review Focus

For firmware changes, reviewers should be able to answer:

- Which build target includes this code?
- Which configuration token or runtime condition activates it?
- What happens on Apple firmware, generic UEFI firmware, and OpenCore
  chain-loads?
- What is the failure mode if the relevant protocol, file, or NVRAM variable is
  missing?

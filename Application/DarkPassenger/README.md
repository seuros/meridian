# DarkPassenger

Reconnaissance for coreboot ports. Vendor firmware routinely ships silicon with
functional blocks fused off. The controllers are on the die and wired to the
board, but the vendor's boot firmware disables them before anything else gets to
look. You cannot bring up in your own firmware what you cannot see from the
vendor's, so the first job on a new port is an inventory of what was turned off
and by which register.

DarkPassenger is that inventory. It runs under the vendor firmware, on the
machine you intend to port, and writes down what that firmware did on its way
up.

## Passengers

One passenger per lockout. Each is a `DARK_PASSENGER` struct with four hooks:

| Hook | Job |
| --- | --- |
| `Applies` | Does this machine have this lockout? Gate on PCI IDs, not on model names. |
| `Recon` | Read and report. Never writes. |
| `Activate` | Clear the lockout. Compiled out of the default build. |
| `Describe` | Report the after state, once `Activate` has run. |

The main loop walks the registry, runs recon for every passenger whose `Applies`
returns true, and writes the transcript to `darkpassenger-app.txt` next to the
binary, falling back to console-only when there is no writable filesystem. A
machine with no applicable passengers says so and exits.

## Read-only by default

**The released build cannot write.** `Activate` is compiled out entirely unless
`DP_ALLOW_REENABLE` is set at build time, so the downloadable binary reports and
changes nothing. The first line of output states the mode, `READ-ONLY` or
`ARMED: writes enabled`.

An armed build is for a bench machine you are prepared to recover. It writes
chipset registers the vendor firmware deliberately left in a particular state,
and the whole point is to find out what happens next.

`DP_ACPI_SELFTEST` is a second flag, and it also requires the armed build. It
injects an inert `DPT0` SSDT (`ssdt_proof.h`) to confirm the ACPI installation
path works before anything real goes through it.

## lpss-lynxpoint

The first passenger covers the Haswell-ULT / Lynx Point-LP LPSS block that
Apple's firmware disables: SDMA, I2C0/I2C1, SPI0/SPI1, UART0/UART1, SDIO. It
gates on the LPC bridge at `00:1F.0` reporting an Intel vendor ID with a device
ID in the Lynx Point-LP range, so it applies to the silicon rather than to a
model list.

On these machines the LPSS functions are dark at PCI config space, `cfg00`
reading `FFFFFFFF`, even though the part is the same one Intel ships to everyone
else.

Recon produces three things:

- The LPC configuration registers: ABASE, ACPI_CNTL, GPIOBASE, RCBA, BIOS_CNTL.
- The per-function IOBP FUNCDIS registers, read through RCBA, showing which
  functions carry the disable bit.
- A walk of the PCI functions showing which are dark and which are live.

That pairing is the useful part. It names the exact register and bit responsible
for each dark function, which is precisely what a coreboot port needs to know so
it can decline to set them.

Armed, it clears `FUNCDIS.DIS` and `PORTCTRL.PCI_CONF_DIS` for each function,
assigns temporary BARs above the PCI Mem32 top, and pings the DesignWare I2C
controllers to confirm they answer. A controller that answers is proof the block
is alive, not merely visible.

## Files

- `DarkPassenger.c`: entry point, registry, report file.
- `dp.h`: the `DARK_PASSENGER` interface and the shared context.
- `dp_util.c`: ACPI dump and install, PCI function lookup, IOBP access, I2C probing.
- `passenger_lpss.c`: the Lynx Point-LP passenger.

## Adding a passenger

Write a new `passenger_<name>.c` with the four hooks, declare its
`DARK_PASSENGER` in `dp.h`, add it to `gDpRegistry` in `DarkPassenger.c`, and
list the source in `DarkPassenger.inf`. Keep `Recon` write-free and put every
write behind `#if DP_ALLOW_REENABLE`, so the released binary stays a tool you
can hand to someone with a machine you have never seen.

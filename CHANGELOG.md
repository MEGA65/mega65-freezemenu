# mega65-freezemenu Changelog

## V0.2.1

* ROMLOAD Function reenabled (requires dev core 2022-11-06 or later)
  - asks user if he wants to reset the system, disabled F3-RESUME if he does not (F4 will RESUME anyways)
  - can also load `*.CHR` files to chargen WOM
  - can also load `*.TCR` files to chargen WOM (8x16 fonts)
* FREEZER has secret F14 charset restore (loads from CHARSET.M65 or MEGA65.ROM)
* backwards compability for charset freezing and restoring for cores earlier than 20230203.20-develo-5f87c76
  - will try to fix slots without charset area using the default charset (see above)

## V0.2.0 (Release 0.95)

Start of this Changelog. See [mega65-freezemenu](https://github.com/MEGA65/mega65-freezemenu/) GIT repo for older changes
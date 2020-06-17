Freeze menu and associated integrated utilities for the MEGA65 (http://github.com/MEGA65).

This program should be installed in the root directory of the MEGA65's SD card
for the MEGA65 to function correctly, as much of the interaction with a MEGA65
is via the freeze menu, e.g., to mount disk images, swap loaded program etc.

This program was started by forking the MEGA65 FDISK/FORMAT utility, since it
has a bunch of the routines we need already in it.

## Prerequisites
* need something for ```pngprepare```, -> ```sudo apt install libpng12-dev```
* the 'cc65' toolchain is required, and by default this is taken care of in the Makefile.
Alternatively, you can supply your pre-built ```cc65``` binaries (see below at "CI").

## Building
* ``make`` will build (including init/update/build of submodule ``./cc65``)

## Continuous Integration (CI)
For CI building, you may not want to build the cc65-submodule,
but rather use a locally installed binary instead.

In that case, build with:
```make USE_LOCAL_CC65=1```

# MEGA65 Freezemenu

Freeze menu and associated integrated utilities for the [MEGA65](http://github.com/MEGA65).

These programs must be installed in the root directory of the MEGA65's SD card
for the MEGA65 to function correctly, as much of the interaction with a MEGA65
is via the freeze menu, e.g., to mount disk images, swap loaded program etc.

The main program that is called by holding RESTORE for a short time is 
`FREEZER.M65`. This will provide a main menu and call the other M65 programs to
provide their functions.

`FREEZER.M65`:
* Main screen, can freeze states to SD card slots and restore them again.
* Provides toggles to change hardware behavior and manipulate slots.
* Lets you change disk unit numbers, mount real drives or disk iamges from SD.
* Shortcuts to start the other utilities
* press `F14` to restore your character set to a sensible default (`CHARSET.M65`
  or `MEGA65.ROM`)

`MEGAINFO.M65`:
* you should use this to file a bug report, so we know what 
  versions you have used
* displays information about your MEGA65, such as
  - Hardware Model
  - Core, HYPPO, ROM, and essential file versions
  - RTC status

`MONITOR.M65`:
* lets you look at the frozen memory

`AUDIOMIX.M65`:
* audio mixer controls

`SPRITED.M65`:
* a simple sprite editor you can use to change the sprites in frozen memory

`ROMLOAD.M65`:
* utility to replace the ROM in the current frozen memory
* also can load *.CHR files directly into chargen WOM

`MAKEDISK.M65`:
* called from the diskchooser within the drive select
* can be used to create an empty disk image

Other things:

`BANNER.M65`:
* the boot banner that is displayed on startup

`CHARSET.M65`:
* not provided with this distribution
* you can place a 4k PETSCII charset here, so you can use `F14` to restore it,
  if some program messed it up
* if not found, freezer tries to load the charset from `MEGA65.ROM` instead.

`xxxTHUMB.M65`:
* frames for the thumbnail display 


## Versions

The latest MEGA65 Freezemenu is always distributed with a core release as the
`SD Card Essentials`. You should normally use the files that come with the core
you use. As the FREEZER depends on functions the CORE provides, using different
combinations might result in problems.

## History

This program was started by forking the MEGA65 FDISK/FORMAT utility, since it
has a bunch of the routines we need already in it.

# Build Process

A Makefile is provided. FREEZER is build automatically by the [core build process](https://builder.mega65.org/job/mega65-core/) and packaged with each
core release package.

## Prerequisites

You can check the
[builder-docker/megabuild](https://github.com/MEGA65/builder-docker/tree/main/megabuild)
Docker container for a approved linux build environment.

Some (probably incopmplete) prerequisites:
* need something for `pngprepare`, -> `sudo apt install libpng12-dev`
* the 'cc65' toolchain is required, and by default this is taken care of in the
  Makefile.
  
  Alternatively, you can supply your pre-built `cc65` binaries (see
  below at "Building").

## Building

Use `make` to build all modules. If you have cc65 installed locally, you can
skip building it by using `make USE_LOCAL_CC65=1` instead.

## Copying to the SD Card

Make sure to always use the rename/delete/copy proces to not produce any
fragmented files on your SD card.

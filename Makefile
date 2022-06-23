
ifdef USE_LOCAL_CC65
	# use locally installed binary (requires cc65 to be in the $PATH)
	CC65=	cc65
	CA65=	ca65
	CL65=	cl65
else
	# use the binary built from the submodule
	CC65=	cc65/bin/cc65
	CA65=	ca65
	CL65=	cc65/bin/cl65
endif

#COPTS=	-t c64 -O -Or -Oi -Os --cpu 65c02 -Icc65/include
COPTS=	-t c64 -Os --cpu 65c02 -Icc65/include
LOPTS=	--asm-include-dir cc65/asminc --cfg-path cc65/cfg --lib-path cc65/lib

FILES=		FREEZER.M65 \
		AUDIOMIX.M65 \
		MONITOR.M65 \
		MAKEDISK.M65 \
		SPRITED.M65 \
		ROMLOAD.M65 \
		MEGAINFO.M65 \
		C65THUMB.M65 \
		C64THUMB.M65 \
		GUSTHUMB.M65

M65IDESOURCES=	freezer.c \
		freeze_audiomix.c \
		frozen_memory.c \
		freeze_monitor.c \
		freeze_diskchooser.c \
		fdisk_memory.c \
		fdisk_screen.c \
		fdisk_fat32.c \
		fdisk_hal_mega65.c

ASSFILES=	freezer.s \
		frozen_memory.s \
		freeze_diskchooser.s \
		fdisk_memory.s \
		fdisk_screen.s \
		fdisk_fat32.s \
		fdisk_hal_mega65.s \
		charset.s \
		helper.s


MONASSFILES=	monitor.s \
		freeze_monitor.s \
		frozen_memory.s \
		fdisk_memory.s \
		fdisk_screen.s \
		fdisk_hal_mega65.s \
		charset.s \
		helper.s

AMASSFILES=	audiomix.s \
		freeze_audiomix.s \
		frozen_memory.s \
		fdisk_memory.s \
		fdisk_screen.s \
		fdisk_hal_mega65.s \
		charset.s \
		helper.s

MDASSFILES=	makedisk.s \
		fdisk_fat32.s \
		frozen_memory.s \
		fdisk_memory.s \
		fdisk_screen.s \
		fdisk_hal_mega65.s \
		charset.s \
		helper.s

SEASSFILES=	sprited.s \
		freeze_sprited.s \
		frozen_memory.s \
		fdisk_memory.s \
		fdisk_screen.s \
		fdisk_hal_mega65.s \
		charset.s \
		helper.s

RLASSFILES=	romload.s \
		freeze_romload.s \
		frozen_memory.s \
		fdisk_memory.s \
		fdisk_screen.s \
		fdisk_hal_mega65.s \
		charset.s \
		helper.s

MIASSFILES=	megainfo.s \
		freeze_megainfo.s \
		frozen_memory.s \
		fdisk_memory.s \
		fdisk_screen.s \
		fdisk_hal_mega65.s \
		charset.s \
		helper.s \
		infohelper.s

LIBCASSFILES=	../mega65-libc/cc65/src/conio.s \
		../mega65-libc/cc65/src/mouse.s

HEADERS=	Makefile \
		freezer.h \
		fdisk_memory.h \
		fdisk_screen.h \
		fdisk_fat32.h \
		fdisk_hal.h \
		ascii.h

DATAFILES=	ascii8x8.bin

%.s:	%.c $(HEADERS) $(DATAFILES) $(CC65)
	$(info ======== Making: $@)
	$(CC65) $(COPTS) --add-source -o $@ $<

all:	$(FILES)

install:	all
	m65ftp < install.mftp

MAKE_VERSION= \
	@if [ -z "$(DO_MKVER)" ] || [ "$(DO_MKVER)" -eq "1" ] ; then \
	echo "Retrieving Git version string... (set env-var DO_MKVER=0 to turn this behaviour off)" ; \
	echo '.segment "CODE"' > version.s ; \
	echo '_version:' >> version.s ; \
	echo "  .asciiz \"v:`./gitversion.sh`\"" >> version.s ; \
	fi


$(CC65):
	$(info ======== Making: $@)
ifdef USE_LOCAL_CC65
	@echo "Using local installed CC65."
else
	git submodule init
	git submodule update
	( cd cc65 && make -j 8 )
endif

ascii8x8.bin: ascii00-7f.png tools/pngprepare
	$(info ======== Making: $@)
	./tools/pngprepare charrom ascii00-7f.png ascii8x8.bin

tools/asciih:	tools/asciih.c
	$(info ======== Making: $@)
	$(CC) -o tools/asciih tools/asciih.c

ascii.h:	tools/asciih
	$(info ======== Making: $@)
	./tools/asciih

tools/pngprepare:	tools/pngprepare.c
	$(info ======== Making: $@)
	$(CC) -I/usr/local/include -L/usr/local/lib -o tools/pngprepare tools/pngprepare.c -lpng

tools/thumbnail-surround-formatter:
	$(info ======== Making: $@)
	gcc -o tools/thumbnail-surround-formatter tools/thumbnail-surround-formatter.c -lpng

FREEZER.M65:	$(ASSFILES) $(DATAFILES) $(CC65)
	$(info ======== Making: $@)
	$(MAKE_VERSION)
	$(CL65) $(COPTS) -g -Ln freezer.lbl $(LOPTS) -vm --add-source -l freezer.list -m freezer.map -o FREEZER.M65 version.s $(ASSFILES)

AUDIOMIX.M65:	$(AMASSFILES) $(DATAFILES) $(CC65)
	$(info ======== Making: $@)
	$(MAKE_VERSION)
	$(CL65) $(COPTS) $(LOPTS) -vm --add-source -l audiomix.list -m audiomix.map -o AUDIOMIX.M65 version.s $(AMASSFILES)

MONITOR.M65:	$(MONASSFILES) $(DATAFILES) $(CC65)
	$(info ======== Making: $@)
	$(MAKE_VERSION)
	$(CL65) $(COPTS) $(LOPTS) -vm --add-source -l monitor.list -m monitor.map -o MONITOR.M65 version.s $(MONASSFILES)

MAKEDISK.M65:	$(MDASSFILES) $(DATAFILES) $(CC65)
	$(info ======== Making: $@)
	$(MAKE_VERSION)
	$(CL65) $(COPTS) $(LOPTS) -vm --add-source -l makedisk.list -m makedisk.map -o MAKEDISK.M65 version.s $(MDASSFILES)

SPRITED.M65:	$(SEASSFILES) $(DATAFILES) $(CC65) $(LIBCASSFILES)
	$(info ======== Making: $@)
	$(MAKE_VERSION)
	$(CL65) $(COPTS) $(LOPTS) -vm --add-source -l sprited.list -m sprited.map -o SPRITED.M65 version.s $(SEASSFILES) $(LIBCASSFILES)

ROMLOAD.M65:	$(RLASSFILES) $(DATAFILES) $(CC65) $(LIBCASSFILES)
	$(info ======== Making: $@)
	$(MAKE_VERSION)
	$(CL65) $(COPTS) $(LOPTS) -vm --add-source -l romload.list -m romload.map -o ROMLOAD.M65 version.s $(RLASSFILES) $(LIBCASSFILES)

MEGAINFO.M65:	$(MIASSFILES) $(DATAFILES) $(CC65) $(LIBCASSFILES)
	$(info ======== Making: $@)
	$(MAKE_VERSION)
	$(CL65) $(COPTS) $(LOPTS) -vm --add-source -l megainfo.list -m megainfo.map -o MEGAINFO.M65 version.s $(MIASSFILES) $(LIBCASSFILES)

M65THUMB.M65:	assets/thumbnail-surround-m65.png tools/thumbnail-surround-formatter
	$(warning ======== Making: $@)
	tools/thumbnail-surround-formatter assets/thumbnail-surround-m65.png M65THUMB.M65 2>/dev/null

C65THUMB.M65:	assets/thumbnail-surround-c65.png tools/thumbnail-surround-formatter
	$(info ======== Making: $@)
	tools/thumbnail-surround-formatter assets/thumbnail-surround-c65.png C65THUMB.M65 2>/dev/null

C64THUMB.M65:	assets/thumbnail-surround-c64.png tools/thumbnail-surround-formatter
	$(info ======== Making: $@)
	tools/thumbnail-surround-formatter assets/thumbnail-surround-c64.png C64THUMB.M65 2>/dev/null

GUSTHUMB.M65:	assets/thumbnail-surround-gus.png tools/thumbnail-surround-formatter
	$(info ======== Making: $@)
	tools/thumbnail-surround-formatter assets/thumbnail-surround-gus.png GUSTHUMB.M65 2>/dev/null

format:
	find . -type d \( -path ./cc65 -o -path ./cbmconvert \) -prune -false -o -iname '*.h' -o -iname '*.c' -o -iname '*.cpp' | xargs clang-format --style=file -i

.PHONY: clean cleangen version.s

clean: cleangen
	rm -f $(FILES) \
	*.o *.map *.list \
	freeze_*.s \
	frozen_*.s \
	audiomix.s makedisk.s monitor.s romload.s sprited.s \
	asciih \
	tools/pngprepare \
	tools/thumbnail-surround-formatter

cleangen:
	rm -f ascii8x8.bin ascii.h

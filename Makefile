
ifdef USE_LOCAL_CC65
	# use locally installed binary (requires cc65 to be in the $PATH)
	CC65=	cc65
	CL65=	cl65
else
	# use the binary built from the submodule
	CC65=	cc65/bin/cc65
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
	$(warning ======== Making: $@)
	$(CC65) $(COPTS) -o $@ $<

all:	$(FILES)

install:	all
	m65ftp < install.mftp

$(CC65):
	$(warning ======== Making: $@)
ifdef USE_LOCAL_CC65
	@echo "Using local installed CC65."
else
	git submodule init
	git submodule update
	( cd cc65 && make -j 8 )
endif

ascii8x8.bin: ascii00-7f.png pngprepare
	$(warning ======== Making: $@)
	./pngprepare charrom ascii00-7f.png ascii8x8.bin

asciih:	asciih.c
	$(warning ======== Making: $@)
	$(CC) -o asciih asciih.c
ascii.h:	asciih
	$(warning ======== Making: $@)
	./asciih

pngprepare:	pngprepare.c
	$(warning ======== Making: $@)
	$(CC) -I/usr/local/include -L/usr/local/lib -o pngprepare pngprepare.c -lpng

FREEZER.M65:	$(ASSFILES) $(DATAFILES) $(CC65)
	$(warning ======== Making: $@)
	$(CL65) $(COPTS) $(LOPTS) -vm --add-source -l freezer.list -m freezer.map -o FREEZER.M65 $(ASSFILES)

AUDIOMIX.M65:	$(AMASSFILES) $(DATAFILES) $(CC65)
	$(warning ======== Making: $@)
	$(CL65) $(COPTS) $(LOPTS) -vm --add-source -l audiomix.list -m audiomix.map -o AUDIOMIX.M65 $(AMASSFILES)

MONITOR.M65:	$(MONASSFILES) $(DATAFILES) $(CC65)
	$(warning ======== Making: $@)
	$(CL65) $(COPTS) $(LOPTS) -vm --add-source -l monitor.list -m monitor.map -o MONITOR.M65 $(MONASSFILES)

MAKEDISK.M65:	$(MDASSFILES) $(DATAFILES) $(CC65)
	$(warning ======== Making: $@)
	$(CL65) $(COPTS) $(LOPTS) -vm --add-source -l makedisk.list -m makedisk.map -o MAKEDISK.M65 $(MDASSFILES)

SPRITED.M65:	$(SEASSFILES) $(DATAFILES) $(CC65) $(LIBCASSFILES)
	$(warning ======== Making: $@)
	$(CL65) $(COPTS) $(LOPTS) -vm --add-source -l sprited.list -m sprited.map -o SPRITED.M65 $(SEASSFILES) $(LIBCASSFILES)

ROMLOAD.M65:	$(RLASSFILES) $(DATAFILES) $(CC65) $(LIBCASSFILES)
	$(warning ======== Making: $@)
	$(CL65) $(COPTS) $(LOPTS) -vm --add-source -l romload.list -m romload.map -o ROMLOAD.M65 $(RLASSFILES) $(LIBCASSFILES)

C65THUMB.M65:	assets/thumbnail-surround-c65.png tools/thumbnail-surround-formatter
	$(warning ======== Making: $@)
	tools/thumbnail-surround-formatter assets/thumbnail-surround-c65.png C65THUMB.M65 2>/dev/null

C64THUMB.M65:	assets/thumbnail-surround-c64.png tools/thumbnail-surround-formatter
	$(warning ======== Making: $@)
	tools/thumbnail-surround-formatter assets/thumbnail-surround-c64.png C64THUMB.M65 2>/dev/null

GUSTHUMB.M65:	assets/thumbnail-surround-gus.png tools/thumbnail-surround-formatter
	$(warning ======== Making: $@)
	tools/thumbnail-surround-formatter assets/thumbnail-surround-gus.png GUSTHUMB.M65 2>/dev/null

tools/thumbnail-surround-formatter:
	$(warning ======== Making: $@)
	gcc -o tools/thumbnail-surround-formatter tools/thumbnail-surround-formatter.c -lpng

format:
	find . -type d \( -path ./cc65 -o -path ./cbmconvert \) -prune -false -o -iname '*.h' -o -iname '*.c' -o -iname '*.cpp' | xargs clang-format --style=file -i

clean:
	rm -f $(FILES) *.o \
	audiomix.s \
	freeze_*.s \
	frozen_memory.s \
	fdisk_*.s \
	freezer.s sprited.s \
	*.map \
	ascii.h ascii8x8.bin asciih \
	pngprepare \
	tools/thumbnail-surround-formatter

cleangen:
	rm ascii8x8.bin

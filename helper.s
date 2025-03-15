
	.setcpu "65C02"
	.export _init_nmi
	.export _hdos_new_attach
	.export _mega65_dos_init
	.export _mega65_geterrorcode
	.export _mega65_dos_attach
	.export _mega65_dos_detach
	.export _mega65_dos_chdir
	.export _mega65_dos_cdroot
	.export _mega65_dos_exechelper
	.export _mega65_dos_getprocdesc
	.export	_fetch_freeze_region_list_from_hypervisor
	.export _find_freeze_slot_start_sector
	.export _unfreeze_slot
	.export _read_file_from_sdcard
	.export _get_freeze_slot_count
	.export _opendir, _readdir, _closedir, _closeall
	.autoimport	on  ;; needed this for jsr incsp2, incsp6
	
	.include "zeropage.inc"
	
.SEGMENT "CODE"

	.p4510
	
  ;; fall-back to FREEZER.M65 if other .M65 file can't be loaded
_freezer_m65:
	.asciiz "freezer.m65"

_init_nmi:
	sei
	lda #<nmi_handler
	sta $318
	lda #>nmi_handler
	sta $319
	rts

nmi_handler:
	rti

_hdos_new_attach:
	.byte 1

_mega65_dos_init:
	;; this checks hyppo dos version for dos_attach
	lda #$00
	sta $D640		; getversion
	clv
	cpy #$01
	bcc @hdos_to_old
	bne @hdos_is_newer
	cpz #$03
	bcc @hdos_to_old
@hdos_is_newer:
	lda #1
	bra @hdos_flag_store
@hdos_to_old:
	lda #0
@hdos_flag_store:
	sta _hdos_new_attach
	ldz #0
	rts

_mega65_geterrorcode:
	;; short mega65_geterrorcode();
	;; Call hypervisor trap
	lda #$38    ; hyppo_geterrorcode - returns errorcode in A
	sta $D642   ; trigger hypervisor trap
	clv         ; dead slot after hypervisor call that must be there to workaround CPU bug
	rts

_mega65_dos_exechelper:
	;; char mega65_dos_exechelper(char *image_name);

	;; Get pointer to file name
	;; sp here is the ca65 sp ZP variable, not the stack pointer of a 4510
	ldy #1
	.p02
	lda (sp),y
	sta ptr1+1
	sta $0141
	dey
	lda (sp),y
	.p4510
	sta ptr1
	sta $0140
	
	;; Copy file name
@NameCopy:
	ldy #0
@NameCopyLoop:
	lda (ptr1),y
	sta $0100,y
	iny
	cmp #0
	bne @NameCopyLoop
	
	;;  Call dos_setname()
	ldy #>$0100
	ldx #<$0100
	lda #$2E     		; dos_setname Hypervisor trap
	sta $D640		; Do hypervisor trap
	clv			; Wasted instruction slot required following hyper trap instruction
	;; XXX Check for error (carry would be clear)

	; close all files to work around hyppo file descriptor leak bug
	lda #$22
	sta $d640
	nop
	
	;; Now copy a little routine into place in $0340 that does the actual loading and jumps into
	;; the program when loaded.
	ldx #$00
@lfr1:	lda loadfile_routine,x
	sta $0340,x
	inx
	cpx #$80
	bne @lfr1

	;; Call helper routine
	jsr $0340

  ;; un-successful? Then try loading freezer instead
	ldz #$00
@loop0:
	ldx #$00
@loop1:
	ldy #$00
@loop2:
	iny
	bne @loop2
	inc $d020   ;; colour-cycle the border to indicate missing file
	inx
	bne @loop1
	inz
	cpz #60
	bne @loop0

	lda #<_freezer_m65
	sta ptr1
	lda #>_freezer_m65
	sta ptr1+1
	bra @NameCopy
	
	;; as this is effectively like exec() on unix, it can only return an error
	lda #$01
	ldx #$00
	
	rts

loadfile_routine:
	; Now load the file to $07ff
	lda #$36
	ldx #$FF
	ldy #$07
	ldz #$00
	sta $d640
	nop
	bcs load_succeeded

	rts

load_succeeded:
	ldz     #$00
	jmp $080d
	rts

attachDrive:
	.byte	$00

_mega65_dos_attach:
	;; Get pointer to file name
	;; sp here is the ca65 sp ZP variable, not the stack pointer of a 4510
	ldy #2
	.p02
	lda (sp),y
	sta ptr1+1
	sta $0441
	dey
	lda (sp),y
	.p4510
	sta ptr1
	sta $0440

	;; Get Drive
	dey
	.p02
	lda (sp),y
	.p4510
	and #$01		; only drive 0 or 1 allowed
	sta attachDrive

	;; Copy file name
	ldy #0
@NameCopyLoop:
	lda (ptr1),y
	sta $0400,y
	iny
	cmp #0
	bne @NameCopyLoop
	
	;;  Call dos_setname()
	ldy #>$0400
	ldx #<$0400
	lda #$2E		; dos_setname Hypervisor trap
	sta $D640
	clv				; Wasted instruction slot required following hyper trap instruction
	bcc @attachError

	lda _hdos_new_attach
	bne @attachNewCall

	;; backwards compability
	lda attachDrive
	beq @attachDrive0
	lda #$06
@attachDrive0:
	clc
	adc #$40		; so this is now $40 or $46
	bra @attachDoHyppoAttach

@attachNewCall:
	;; Now we call dos_attach
	ldx attachDrive
	lda #$4A		; dos_attach Hypervisor trap
@attachDoHyppoAttach:
	sta $D640
	clv
	bcc @attachError
	lda #$00
	bra @attachExit
@attachError:
	lda #$ef
@attachExit:
	jmp incsp3		; remove the args from the stack and return

_mega65_dos_detach:
	;; char mega65_dos_detach(uint8_t drive)
	;; argument is passed in A
	and #$41		; only drive 0 or 1 allowed, also allow bit 6 - nodrive
	ora #$80		; set bit 7 for detach operation
	tax
	lda _hdos_new_attach
	bne @detach_new
	lda #$42		; dos_d81detach Hypervisor trap
	bra @detach_call
@detach_new:
	lda #$4A		; dos_attach Hypervisor trap (which does it all)
@detach_call:
	sta $D640
	clv
	rts

_mega65_dos_chdir:
	;; char mega65_dos_chdir(char *dir_name);

	;; Get pointer to file name
	;; sp here is the ca65 sp ZP variable, not the stack pointer of a 4510
	ldy #1
	.p02
	lda (sp),y
	sta ptr1+1
	sta $0441
	dey
	lda (sp),y
	.p4510
	sta ptr1
	sta $0440
	
	;; Copy file name
	ldy #0
@NameCopyLoop:
	lda (ptr1),y
	sta $0400,y
	iny
	cmp #0
	bne @NameCopyLoop
	
	;;  Call dos_setname()
	ldy #>$0400
	ldx #<$0400
	lda #$2E     		; dos_setname Hypervisor trap
	sta $D640		; Do hypervisor trap
	clv			; Wasted instruction slot required following hyper trap instruction
	bcc @direntNotFound

	;; Find the file
	lda #$34
	sta $D640
	clv
	bcc @direntNotFound

	;; Try to change directory to it
	lda #$0C
	sta $D640
	clv

@direntNotFound:
	;; store flags
	php
	
	jsr incsp2  ; remove the char* arg from the stack

	;; return inverted carry flag, so result of 0 = success
	pla
	and #$01
	eor #$01
	ldx #$00
	
	rts
	
_mega65_dos_cdroot:
	;; char mega65_dos_cdroot();
	;; Call hypervisor trap
	lda #$04    ; hyppo_dos_getcurrentdrive - returns drive number in A
	sta $D640   ; trigger hypervisor trap
	clv
	bcc chroot_error
	tax         ; next need drive in X
	lda #$3C    ; hyppo_dos_cdrootdir - returns errorcode in A
	sta $D640   ; trigger hypervisor trap
	clv
	bcc chroot_error
	lda #$00
	rts
chroot_error:
	lda #$01
	rts


_mega65_dos_getprocdesc:
	;; uint8_t mega65_dos_getprocdesc(uint8_t pagemsb)
	tay
	lda #$48
	sta $D640	; call hyppo
	clv
	bcc @error
	lda #$00
	rts
@error:
	lda #$01
	rts


_unfreeze_slot:	

	;; Move 16-bit address from A/X to X/Y
	phx
	tay
	pla
	tax

	;; Call hypervisor trap
	lda #$12    ; subfunction for syspart trap to unfreeze from a slot
	sta $D642   ; trigger hypervisor trap
	clv         ; dead slot after hypervisor call that must be there to workaround CPU bug
	rts

_get_freeze_slot_count:	

	;; Call hypervisor trap
	lda #$16    ; subfunction for syspart trap to get freeze region list
	sta $D642   ; trigger hypervisor trap
	clv         ; dead slot after hypervisor call that must be there to workaround CPU bug

	txa
	phy
	plx
	
	rts
	
_fetch_freeze_region_list_from_hypervisor:

	;; Move 16-bit address from A/X to X/Y
	phx
	tax
	pla
	tay

	;; Call hypervisor trap
	lda #$14    ; subfunction for syspart trap to get freeze region list
	sta $D642   ; trigger hypervisor trap
	clv         ; dead slot after hypervisor call that must be there to workaround CPU bug
	rts

_find_freeze_slot_start_sector:	

	;; Move 16-bit address from A/X to X/Y
	;; XXX - We had to swap the X/Y byte order around for this to work: Why???
	phx
	tay
	pla
	tax

	;; Call hypervisor trap
	lda #$10    ; subfunction for syspart trap to put start sector of freeze slot into $D681-$D684
	sta $D642   ; trigger hypervisor trap
	clv         ; dead slot after hypervisor call that must be there to workaround CPU bug
	rts
	

_read_file_from_sdcard:

	;; char read_file_from_sdcard(char *filename,uint32_t load_address);

	;; Hypervisor requires copy area to be page aligned, so
	;; we have to copy the name we want to load to somewhere on a page boundary
	;; This is a bit annoying.  I should find out why I made the hypervisor make
	;; such an requirement.  Oh, and it also has to be in the bottom 32KB of memory
	;; (that requirement makes more sense, as it is about ensuring that the
	;; Hypervisor can't be given a pointer that points into its own mapped address space)
	;; As we are not putting any screen at $0400, we can use that
	
	;; Get pointer to file name
	;; sp here is the ca65 sp ZP variable, not the stack pointer of a 4510
	ldy #5
	.p02
	lda (sp),y
	sta ptr1+1
	dey
	lda (sp),y
	.p4510
	sta ptr1

	;; Copy file name
	ldy #0
@NameCopyLoop:
	lda (ptr1),y
	sta $0400,y
	iny
	cmp #0
	bne @NameCopyLoop
	
	;;  Call dos_setname()
	ldy #>$0400
	ldx #<$0400
	lda #$2E     		; dos_setname Hypervisor trap
	sta $D640		; Do hypervisor trap
	clv			; Wasted instruction slot required following hyper trap instruction
	bcc @readfileError

	;; Get Load address into $00ZZYYXX
	ldy #2
	.p02
	lda (sp),y
	.p4510
	taz
	ldy #0
	.p02
	lda (sp),y
	tax
	iny
	lda (sp),y
	.p4510
	tay

	;; Ask hypervisor to do the load
	lda #$36
	sta $D640
	clv

@readfileError:
	;; store flags
	php

	jsr incsp6

	;; return inverted carry flag, so result of 0 = success
	pla
	and #$01
	eor #$01
	ldx #$00
	ldz #$00
	
	rts

_closeall:
	; close all files to work around hyppo file descriptor leak bug
	lda #$22
	sta $d640
	clv
	rts
	
	;; closedir takes file descriptor as argument (appears in A)
_closedir:
	tax
	lda #$16
	sta $D640
	clv
	ldx #$00
	rts
	
	;; Opendir takes no arguments and returns File descriptor in A
_opendir:
	lda #$12
	sta $D640
	clv
	ldx #$00
	rts

	;; readdir takes the file descriptor returned by opendir as argument
	;; and gets a pointer to a MEGA65 DOS dirent structure.
	;; Again, the annoyance of the MEGA65 Hypervisor requiring a page aligned
	;; transfer area is a nuisance here. We will use $0400-$04FF, and then
	;; copy the result into a regular C dirent structure
	;;
	;; d_ino = first cluster of file
	;; d_off = offset of directory entry in cluster
	;; d_reclen = size of the dirent on disk (32 bytes)
	;; d_type = file/directory type
	;; d_name = name of file
_readdir:

	pha
	
	;; First, clear out the dirent
	ldx #0
	txa
@l1:	sta _readdir_dirent,x	
	dex
	bne @l1

	;; Third, call the hypervisor trap
	;; File descriptor gets passed in in X.
	;; Result gets written to transfer area we setup at $0400
	plx
	ldy #>$0400 		; write dirent to $0400 
	lda #$14
	sta $D640
	clv

	bcs @readDirSuccess

	;;  Return end of directory
	lda #$00
	ldx #$00
	rts

@readDirSuccess:
	
	;;  Copy file name
	ldx #$3f
@l2:
	lda $0400,x
	sta _readdir_dirent+4+2+4+2,x
	dex
	bpl @l2
	;; make sure it is null terminated
	ldx $0400+64
	lda #$00
	sta _readdir_dirent+4+2+4+2,x

	;; Inode = cluster from offset 64+1+12 = 77
	ldx #$03
@l3:
	lda $0477,x
	sta _readdir_dirent+0,x
	dex
	bpl @l3

	;; d_off stays zero as it is not meaningful here
	
	;; d_reclen we preload with the length of the file (this saves calling stat() on the MEGA65)
	ldx #3
@l4:
	lda $0400+64+1+12+4,x
	sta _readdir_dirent+4+2,x
	dex
	bpl @l4

	;; File type and attributes
	;; XXX - We should translate these to C style meanings
	lda $0400+64+1+12+1+4+4
	sta _readdir_dirent+4+2+4

	;; Return address of dirent structure
	lda #<_readdir_dirent
	ldx #>_readdir_dirent
	
	rts

_readdir_dirent:
	.dword 0   		; d_ino
	.word 0			; d_off
	.dword 0		; d_reclen
	.word 0			; d_type
	.res 256,$00

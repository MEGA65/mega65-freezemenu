
	.setcpu "65C02"
	.export _mega65_dos_d81attach0
	.export _mega65_dos_d81attach1
	.export _mega65_dos_chdir
	.export _mega65_dos_exechelper
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
	STA $D640		; Do hypervisor trap
	NOP			; Wasted instruction slot required following hyper trap instruction
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
	LDA #$01
	LDX #$00
	
	RTS

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

attachHyppoCmd:
	.byte	$40

_mega65_dos_d81attach1:
	;; char mega65_dos_d81attach1(char *image_name);
	lda #$46
	sta attachHyppoCmd
	bra attachLoadFN

_mega65_dos_d81attach0:
	;; char mega65_dos_d81attach0(char *image_name);
	lda #$40
	sta attachHyppoCmd

attachLoadFN:
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
	STA $D640		; Do hypervisor trap
	NOP			; Wasted instruction slot required following hyper trap instruction
	bcc @attachError

	;; Try to attach it
	LDA attachHyppoCmd
	STA $D640
	NOP

@attachError:
	;; save flags
	PHP

	JSR incsp2  ; remove the char* arg from the stack
	
	;; return inverted carry flag, so result of 0 = success
	PLA
	AND #$01
	EOR #$01
	LDX #$00

	RTS

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
	STA $D640		; Do hypervisor trap
	NOP			; Wasted instruction slot required following hyper trap instruction
	bcc @direntNotFound

	;; Find the file
	LDA #$34
	STA $D640
	NOP
	BCC @direntNotFound

	;; Try to change directory to it
	LDA #$0C
	STA $D640
	NOP

@direntNotFound:
	;; store flags
	PHP
	
	jsr incsp2  ; remove the char* arg from the stack

	;; return inverted carry flag, so result of 0 = success
	PLA
	AND #$01
	EOR #$01
	LDX #$00
	
	RTS
	

	
_unfreeze_slot:	

	;; Move 16-bit address from A/X to X/Y
	PHX
	TAY
	PLA
	TAX

	;; Call hypervisor trap
	LDA #$12    ; subfunction for syspart trap to unfreeze from a slot
	STA $D642   ; trigger hypervisor trap
	NOP         ; dead slot after hypervisor call that must be there to workaround CPU bug
	RTS

_get_freeze_slot_count:	

	;; Call hypervisor trap
	LDA #$16    ; subfunction for syspart trap to get freeze region list
	STA $D642   ; trigger hypervisor trap
	NOP         ; dead slot after hypervisor call that must be there to workaround CPU bug

	txa
	phy
	plx
	
	RTS	
	
_fetch_freeze_region_list_from_hypervisor:

	;; Move 16-bit address from A/X to X/Y
	PHX
	TAX
	PLA
	TAY

	;; Call hypervisor trap
	LDA #$14    ; subfunction for syspart trap to get freeze region list
	STA $D642   ; trigger hypervisor trap
	NOP         ; dead slot after hypervisor call that must be there to workaround CPU bug
	RTS

_find_freeze_slot_start_sector:	

	;; Move 16-bit address from A/X to X/Y
	;; XXX - We had to swap the X/Y byte order around for this to work: Why???
	PHX
	TAY
	PLA
	TAX

	;; Call hypervisor trap
	LDA #$10    ; subfunction for syspart trap to put start sector of freeze slot into $D681-$D684
	STA $D642   ; trigger hypervisor trap
	NOP         ; dead slot after hypervisor call that must be there to workaround CPU bug
	RTS
	

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
	STA $D640		; Do hypervisor trap
	NOP			; Wasted instruction slot required following hyper trap instruction
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
	LDA #$36
	STA $D640		
	NOP

@readfileError:
	;; store flags
	PHP

	jsr incsp6

	;; return inverted carry flag, so result of 0 = success
	PLA
	AND #$01
	EOR #$01
	LDX #$00
	LDZ #$00
	
	RTS

_closeall:
	; close all files to work around hyppo file descriptor leak bug
	lda #$22
	sta $d640
	nop

	rts
	
	;; closedir takes file descriptor as argument (appears in A)
_closedir:
	TAX
	LDA #$16
	STA $D640
	NOP
	LDX #$00
	RTS
	
	;; Opendir takes no arguments and returns File descriptor in A
_opendir:
	LDA #$12
	STA $D640
	NOP
	LDX #$00
	RTS

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
	STA $D640
	NOP

	bcs @readDirSuccess

	;;  Return end of directory
	lda #$00
	ldx #$00
	RTS

@readDirSuccess:
	
	;;  Copy file name
	ldx #$3f
@l2:	lda $0400,x
	sta _readdir_dirent+4+2+4+2,x
	dex
	bpl @l2
	;; make sure it is null terminated
	ldx $0400+64
	lda #$00
	sta _readdir_dirent+4+2+4+2,x

	;; Inode = cluster from offset 64+1+12 = 77
	ldx #$03
@l3:	lda $0477,x
	sta _readdir_dirent+0,x
	dex
	bpl @l3

	;; d_off stays zero as it is not meaningful here
	
	;; d_reclen we preload with the length of the file (this saves calling stat() on the MEGA65)
	ldx #3
@l4:	lda $0400+64+1+12+4,x
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
	
	RTS

_readdir_dirent:
	.dword 0   		; d_ino
	.word 0			; d_off
	.dword 0		; d_reclen
	.word 0			; d_type
	.res 256,$00

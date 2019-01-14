
	.export	_fetch_freeze_region_list_from_hypervisor
	.export _find_freeze_slot_start_sector
	.export _unfreeze_slot
	.export _read_file_from_sdcard

	.include "zeropage.inc"
	
.SEGMENT "CODE"

_unfreeze_slot:	

	;; Move 16-bit address from A/X to X/Y
	PHX
	TAX
	PLA
	TAY

	;; Call hypervisor trap
	LDA #$12    ; subfunction for syspart trap to unfreeze from a slot
	STA $D642   ; trigger hypervisor trap
	NOP         ; dead slot after hypervisor call that must be there to workaround CPU bug
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
	PHX
	TAX
	PLA
	TAY

	;; Call hypervisor trap
	LDA #$10    ; subfunction for syspart trap to put start sector of freeze slot into $D681-$D684
	STA $D642   ; trigger hypervisor trap
	NOP         ; dead slot after hypervisor call that must be there to workaround CPU bug
	RTS
	

_read_file_from_sdcard:

	;;  read_file_from_sdcard(char *filename,uint32_t load_address);

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
	lda (sp),y
	sta ptr1+1
	dey
	lda (sp),y
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
	;; XXX Check for error (carry would be clear)

	;; Get Load address into $00ZZYYXX
	ldy #2
	lda (sp),y
	.p4510	
	taz
	.p02
	ldy #0
	lda (sp),y
	tax
	iny
	lda (sp),y
	tay

	;; Ask hypervisor to do the load
	LDA #$36
	STA $D640		
	NOP
	;; XXX Check for error (carry would be clear)

	.p4510
	LDZ #$00
	.p02
	
	RTS

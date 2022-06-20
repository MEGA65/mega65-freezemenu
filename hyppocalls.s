
	.setcpu "65C02"
	.export _hyppo_getversion
	.export _hyppo_opendir
	.export _hyppo_readdir
	.export _hyppo_closedir
	.autoimport	on  ;; needed this for jsr incsp2
	
	.include "zeropage.inc"

.SEGMENT "CODE"

	.p4510

_hyppo_opendir:
  lda #$12
  sta $d640 ; trap_dos_opendir
  nop

	;; return inverted carry flag, so result of 0 = success
	php
	pla
	and #$01
	eor #$01
	ldx #$00

	rts

_hyppo_readdir:
  ldx #$00  ; assuming we've got file desciptor 0
  ldy #$04  ; load it to $0400
  lda #$14  ; trap-dos_readdir
  sta $d640
  nop

	;; return inverted carry flag, so result of 0 = success
	php
	pla
	and #$01
	eor #$01
	ldx #$00

	rts

_hyppo_closedir:
  ldx #$00
  lda #$16
  sta $d640 ; trap_dos_closedir
  nop

	;; return inverted carry flag, so result of 0 = success
	php
	pla
	and #$01
	eor #$01
	ldx #$00

	rts

_hyppo_getversion:
	;; char hyppo_get_version(unsigned char *buffer);

	;; Get HYPPO Version and write bytes to buffer
	;; buffer needs to be 4 byte large!
	ldy #1
	.p02
	lda (sp),y
	sta ptr1+1
	dey
	lda (sp),y
	.p4510
	sta ptr1

        ;; call hyppo get version
	lda #$00     		; hyppo_getversion Hypervisor trap
	sta $D640		; Do hypervisor trap
	nop
        nop
        nop
        nop

	;; Copy Result
        phy
	ldy #0
	sta (ptr1),y
	iny
        txa
        sta (ptr1),y
        iny
        pla
        sta (ptr1),y
        iny
        tza
        sta (ptr1),y

  	jsr incsp2  ; remove the char* arg from the stack
	
	;; return inverted carry flag, so result of 0 = success
	php
	pla
	and #$01
	eor #$01
	ldx #$00

	rts

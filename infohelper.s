
	.setcpu "65C02"
	.export _hyppo_getversion
	.autoimport	on  ;; needed this for jsr incsp2
	
	.include "zeropage.inc"

.SEGMENT "CODE"

	.p4510

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
	clv	

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

	ldz #0

	rts

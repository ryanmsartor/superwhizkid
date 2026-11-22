; Independently authored experimental Gamate firmware.
; SPDX-License-Identifier: GPL-2.0-or-later

.setcpu "6502"

.segment "TABLE"
	jmp draw_placeholder        ; $F000
	jmp unsupported             ; $F003
	jmp unsupported             ; $F006
	jmp wait_24_irqs            ; $F009
	jmp clear_display           ; $F00C
	jmp unsupported             ; $F00F
	jmp draw_glyph_run          ; $F012
	jmp unsupported             ; $F015
	jmp unsupported             ; $F018
	jmp unsupported             ; $F01B
	jmp draw_tile_stream        ; $F01E
	jmp unsupported             ; $F021
	jmp initialize_records      ; $F024
	jmp unsupported             ; $F027
	jmp fill_rectangle          ; $F02A
	jmp unsupported             ; $F02D
	jmp unsupported             ; $F030
	jmp unsupported             ; $F033

.segment "TRAPS"
unsupported:
	jmp unsupported
bad_protection:
	jmp bad_protection
bad_identity:
	jmp bad_identity
bad_checksum:
	jmp bad_checksum

.segment "CODE"
reset:
	sei
	cld
	ldx #$FF
	txs

	lda #$FF
	ldx #$00
@fill_ram:
	sta $0000,x
	sta $0100,x
	sta $0200,x
	sta $0300,x
	inx
	bne @fill_ram
	lda #$00
	ldx #$1F
@clear_low_ram:
	sta $0000,x
	dex
	bpl @clear_low_ram

	ldx #$00
@send_byte:
	lda protection_identifier,x
	sta $00
	ldy #$08
@send_bit:
	asl $00
	lda #$00
	rol a
	asl a
	asl a
	sta $6000
	dey
	bne @send_bit
	sta $6000
	inx
	cpx #$0F
	bne @send_byte

	lda #$00
	sta $04
	ldy #$08
@receive_bit:
	lda $6000
	lsr a
	lsr a
	rol $04
	dey
	bne @receive_bit
	lda $04
	cmp #$47
	beq :+
	jmp bad_protection
:
	ldx #$18
@check_identity:
	lda $6005,x
	cmp cartridge_identity,x
	beq :+
	jmp bad_identity
:
	dex
	bpl @check_identity

	lda #$00
	sta $00
	sta $01
	sta $02
	lda #$70
	sta $03
	ldy #$00
@checksum_byte:
	lda ($02),y
	clc
	adc $00
	sta $00
	bcc :+
	inc $01
:
	iny
	bne @checksum_byte
	inc $03
	lda $03
	cmp #$80
	bne @checksum_byte
	lda $00
	cmp $6000
	beq :+
	jmp bad_checksum
:
	lda $01
	cmp $6001
	beq :+
	jmp bad_checksum
:
	lda $6000
	sta $E0
	sta $E5
	lda $6001
	sta $E1
	sta $E6

	lda #$00
	sta $0C
	sta $0B
	ldx #$0F
@clear_ay:
	sta $4000,x
	dex
	bpl @clear_ay
	sta $5001
	cli
	lda #$00
	jsr clear_plane
	lda #$80
	jsr clear_plane

	lda $0B
@wait_for_irq:
	cmp $0B
	beq @wait_for_irq

	lda #$00
	ldx #$1F
@initialize_low_ram:
	sta $0000,x
	dex
	bpl @initialize_low_ram
	sta $21
	sta $5002
	lda #$47
	sta $04
	lda #$FF
	ldx #$05
@initialize_sentinels:
	sta $0000,x
	inx
	cpx #$0B
	bne @initialize_sentinels
	lda #$03
	sta $10
	lda #$94
	sta $11
	lda #$8A
	sta $12
	lda #$35
	sta $13
	lda #$5F
	sta $5003
	lda #$40
	sta $5001
	lda #$8E
	sta $5004
	lda #$C6
	sta $5005
	lda #$01
	sta $0C
	ldx #$28
	ldy #$FF
	lda #$FF
	clv
	cld
	sec
	jmp $6020

clear_plane:
	sta $5004
	lda #$00
	sta $5005
	ldx #$20
	ldy #$00
@byte:
	sta $5007
	iny
	bne @byte
	dex
	bne @byte
	rts

irq:
	pha
	tya
	pha
	inc $0B
	inc $11
	bne @callback
	inc $10
	bne @callback
	inc $0F
	bne @callback
	inc $0E
@callback:
	lda $0C
	beq :+
	jsr $6026
:
	pla
	tay
	pla
	rti

wait_24_irqs:
	lda $0B
	cmp #$18
	bcc wait_24_irqs
	lda #$00
	sta $0B
	sec
	rts

initialize_records:
	ldx #$00
@record:
	lda #$FF
	sta $23,x
	sta $28,x
	txa
	clc
	adc #$08
	tax
	cpx #$80
	bne @record
	ldx #$00
	ldy #$80
	lda #$80
	bit overflow_flag
	clc
	rts

clear_display:
	lda $15
	ora #$80
	sta $5001
	lda #$00
	sta $1E
	jsr clear_display_plane
	lda #$80
	sta $1E
	jsr clear_display_plane
	lda $15
	ora #$40
	sta $5001
	sec
	rts

clear_display_plane:
	ldy #$00
@row:
	lda $1E
	sta $5004
	tya
	sta $5005
	ldx #$20
	lda #$00
@column:
	sta $5007
	dex
	bne @column
	iny
	cpy #$C8
	bne @row
	rts

draw_placeholder:
	lda $15
	ora #$40
	sta $5001
	ldx #$00
@column:
	txa
	and #$03
	ora #$08
	cpx #$04
	bcc :+
	ora #$80
:
	sta $5004
	lda #$8C
	sta $5005
	lda #$00
	ldy #$08
@row:
	sta $5007
	dey
	bne @row
	inx
	cpx #$08
	bne @column
	lda #$38
	ldx #$40
	ldy #$20
	bit zero_flag
	clc
	rts

draw_glyph_run:
	txa
	lsr a
	lsr a
	lsr a
	ora #$80
	sta $1A
	sty $1B
	ldy #$00
	lda ($00),y
	pha
	iny
@tile:
	lda $15
	ora #$40
	sta $5001
	lda $1A
	sta $5004
	lda $1B
	sta $5005
	lda ($00),y
	pha
	tya
	pha
	tsx
	inx
	inx
	lda $0100,x
	tax
	cpx #$41
	bcs @later_group
	lda #$21
	sta $18
	lda #$60
	sta $19
	cpx #$21
	bcs @first_group_index
	lda #$08
	bne @glyph_index
@first_group_index:
	txa
	sec
	sbc #$2E
	jmp @scale_index
@later_group:
	cpx #$61
	bcs @last_group
	lda #$79
	sta $18
	lda #$60
	sta $19
	jmp @indexed_group
@last_group:
	lda #$49
	sta $18
	lda #$61
	sta $19
@indexed_group:
	txa
@scale_index:
	asl a
	asl a
	asl a
@glyph_index:
	tay
	bne :+
	inc $19
:
	tya
	pha
	ldx #$08
@glyph_row:
	lda ($18),y
	sta $5007
	iny
	dex
	bne @glyph_row
	pla
	tax
	pla
	tay
	pla

	cmp #$21
	bcc @overflow_done
	cmp #$80
	bcc @clear_overflow
	cmp #$E1
	bcs @clear_overflow
	bit overflow_flag
	jmp @overflow_done
@clear_overflow:
	clv
@overflow_done:
	cmp #$20
	bcc @clear_carry
	cmp #$2F
	bcc @set_carry
	cmp #$81
	bcc @clear_carry
	cmp #$A1
	bcc @set_carry
	cmp #$C1
	bcc @clear_carry
	cmp #$E1
	bcc @set_carry
@clear_carry:
	clc
	jmp @carry_done
@set_carry:
	sec
@carry_done:
	inc $1A
	php
	txa
	pha
	tsx
	inx
	inx
	inx
	tya
	cmp $0100,x
	beq @finished
	pla
	tax
	plp
	iny
	jmp @tile
@finished:
	pla
	tax
	plp
	pla
	tay
	sta $18
	php
	pla
	and #$7D
	ora #$02
	pha
	lda $18
	plp
	rts

draw_tile_stream:
	lda $15
	ora #$40
	sta $5001
	lda $01
	pha
	lda #$00
	jsr tile_stream_pass
	pla
	sta $01
	lda #$02
	jsr tile_stream_pass
	lda #$FE
	cmp #$FE
	rts

tile_stream_pass:
	pha
	ldy #$00
@row:
	lda ($00),y
	cmp #$FE
	bne :+
	jmp @end
:
	sta $19
	txa
	pha
	tsx
	inx
	inx
	lda $0100,x
	beq :+
	lda #$80
:
	sta $18
	pla
	tax
	iny
	bne @tile
	inc $01
@tile:
	lda ($00),y
	cmp #$FF
	beq @end_row
	sta $1A
	txa
	pha
	tsx
	inx
	inx
	lda $0100,x
	tax
	lda $06,x
	sta $02
	lda $07,x
	sta $03
	lda $1A
	lsr a
	lsr a
	lsr a
	lsr a
	lsr a
	tax
	beq :+
	clv
@page:
	inc $03
	dex
	bne @page
:
	lda $1A
	beq @skip_tile
	asl a
	asl a
	asl a
	sta $1A
	lda #$00
	sta $1B
	lda $18
	sta $5004
	lda $19
	asl a
	asl a
	asl a
	sta $5005
	tya
	pha
	ldy $1A
	ldx #$08
@tile_row:
	lda ($02),y
	sta $5007
	iny
	dex
	bne @tile_row
	pla
	tay
	pla
	ldx #$FF
	inc $18
	jmp @next_tile
@skip_tile:
	lda #$01
	sta $1B
	lda $18
	clc
	adc #$01
	sta $18
	pla
	tax
@next_tile:
	iny
	bne @tile
	inc $01
	jmp @tile
@end_row:
	iny
	bne :+
	inc $01
:
	jmp @row
@end:
	pla
	rts

fill_rectangle:
	sta $1A
	txa
	lsr a
	lsr a
	lsr a
	sta $18
	sty $19
	lda #$00
	sta $A8
	lda $15
	ora #$40
	sta $5001
	lda $1C
	lsr a
	lsr a
	lsr a
	tax
@column:
	lda $18
	sta $5004
	lda $19
	sta $5005
	ldy $1D
	lda $1A
@row:
	sta $5007
	dey
	bne @row
	inc $18
	dex
	bne @column
	lda $1A
	ldx #$00
	ldy #$00
	clc
	rts

overflow_flag:
	.byte $40
zero_flag:
	.byte $00
protection_identifier:
	.byte "BIT CORPORATION"
cartridge_identity:
	.byte "COPYRIGHT BIT CORPORATION"

.segment "VECTORS"
	.word unsupported
	.word reset
	.word irq

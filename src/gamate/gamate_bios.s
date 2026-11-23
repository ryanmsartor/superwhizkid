; Copyright (c) 2026 Ry
; Independently authored experimental Gamate firmware.
; SPDX-License-Identifier: GPL-2.0-or-later

.setcpu "6502"

.segment "TABLE"
	jmp draw_placeholder        ; $F000
	jmp wait_8_irqs             ; $F003
	jmp wait_counter            ; $F006
	jmp wait_24_irqs            ; $F009
	jmp clear_display           ; $F00C
	jmp clear_fixed_rectangle   ; $F00F
	jmp draw_glyph_run          ; $F012
	jmp copy_bitmap             ; $F015
	jmp draw_pair_stream        ; $F018
	jmp draw_row_runs           ; $F01B
	jmp draw_tile_stream        ; $F01E
	jmp draw_row_literals       ; $F021
	jmp initialize_records      ; $F024
	jmp composite_objects       ; $F027
	jmp fill_rectangle          ; $F02A
	jmp fill_rectangle_plane1   ; $F02D
	jmp next_random             ; $F030
	jmp format_digits           ; $F033

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

nmi:
	pha
	lda $4800
	sta $0A
	lda $0C
	beq :+
	jsr $6023
:
	pla
	rti

wait_8_irqs:
	lda $0B
	cmp #$08
	bcc wait_8_irqs
	lda #$00
	sta $0B
	sec
	rts

wait_24_irqs:
	lda $0B
	cmp #$18
	bcc wait_24_irqs
	lda #$00
	sta $0B
	sec
	rts

wait_counter:
	lda $0B
	cmp #$05
	bcc wait_counter
	lda #$00
	sta $0B
	rts

; Advances the 16-bit state at $12-$13: feedback is bit 4 XOR bit 10, and zero restarts at $FFFF.
next_random:
	lda $12
	ora $13
	bne :+
	lda #$FF
	sta $12
	sta $13
:
	lda #$00
	sta $14
	lda $13
	and #$04
	beq :+
	lda #$80
	sta $14
:
	lda $12
	asl a
	asl a
	asl a
	eor $14
	asl a
	rol $12
	rol $13
	lda $12
	rts

; Formats six packed nibbles from A, X and Y as a length-prefixed string at $A1-$A7.
format_digits:
	pha
	lsr a
	lsr a
	lsr a
	lsr a
	ora #$30
	sta $A2
	pla
	and #$0F
	ora #$30
	sta $A3
	txa
	lsr a
	lsr a
	lsr a
	lsr a
	ora #$30
	sta $A4
	txa
	and #$0F
	ora #$30
	sta $A5
	cpy #$FF
	bne @digits
	lda #$20
	sta $A6
	sta $A7
	.repeat 2
	nop
	.endrepeat
	bne @blank
@digits:
	tya
	lsr a
	lsr a
	lsr a
	lsr a
	ora #$30
	sta $A6
	tya
	and #$0F
	ora #$30
	sta $A7
	.repeat 3
	nop
	.endrepeat
@blank:
	ldy #$00
@scan:
	lda $00A2,y
	cmp #$30
	bne @done
	lda #$20
	sta $00A2,y
	iny
	cpy #$05
	bne @scan
@done:
	lda #$06
	sta $A1
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
	.repeat 11
	nop
	.endrepeat
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

copy_plane:
@column:
	lda $018
	sta $5004
	lda $019
	sta $5005
	ldy #$00
	ldx $01D
@row:
	lda ($1E),y
	sta $5007
	sta $01B
	iny
	dex
	bne @row
	lda $01D
	bne @advance
	inc $1F
	jmp @next_column
@advance:
	lda $1E
	clc
	adc $01D
	sta $1E
	bcc @next_column
	inc $1F
@next_column:
	lda $018
	clc
	adc #$01
	sta $018
	dec $01C
	bne @column
	rts

copy_bitmap:
	txa
	pha
	php
	pla
	sta $01A
	lda $015
	ora #$40
	sta $5001
	lda $01C
	lsr a
	lsr a
	lsr a
	sta $01C
	sty $019
	txa
	lsr a
	lsr a
	lsr a
	sta $018
	lda $000
	sta $1E
	lda $001
	sta $1F
	lda $01C
	pha
	jsr copy_plane
	pla
	sta $01C
	lda $002
	sta $000
	lda $003
	sta $001
	lda $002
	sta $1E
	lda $003
	sta $1F
	lda $018
	sec
	sbc $01C
	ora #$80
	sta $018
	lda $01C
	pha
	jsr copy_plane
	pla
	sta $01C
	lda $002
	sta $000
	lda $003
	sta $001
	lda #$00
	ldy $01C
@multiply:
	clc
	adc $01D
	dey
	bne @multiply
	tay
	pla
	tax
	lda $01A
	and #$4C
	ora #$02
	pha
	lda $01B
	plp
	rts

clear_fixed_rectangle:
	lda $15
	and #$BF
	sta $5001
	lda #$00
	sta $A8
	sta $1A
	lda #$1F
	sta $18
	lda #$E6
	sta $19
	lda #$FF
	sta $1C
	lda #$19
	sta $1D
	lda #$00
	ldy #$E6
@row:
	sta $5004
	tya
	sta $5005
	ldx #31
	lda #$00
@column:
	sta $5007
	dex
	bne @column
	iny
	cpy #$FF
	bne @row
	lda $15
	ora #$40
	sta $5001
	lda #$1E
	sta $5004
	lda #$FF
	sta $5005
	; Neutral cycle padding for the measured fixed-rectangle ready path.
.repeat 206
	nop
.endrepeat
	lda #$00
	tax
	tay
	sec
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

; Two-pass row stream of (tile, count) pairs; tile zero advances the column by count.
draw_pair_stream:
	lda $15
	ora #$40
	sta $5001
	lda $01
	pha
	lda $00
	pha
	lda #$00
	jsr pair_stream_pass
	pla
	sta $00
	pla
	sta $01
	lda #$02
	jsr pair_stream_pass
	lda #$FE
	cmp #$FE
	rts

pair_stream_pass:
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
	lda $0102,x
	beq :+
	lda #$80
:
	sta $18
	pla
	tax
@pair:
	iny
	bne :+
	inc $01
:
	lda ($00),y
	cmp #$FF
	bne :+
	jmp @end_row
:
	sta $1A
	iny
	bne :+
	inc $01
:
	lda ($00),y
	sta $1B
	txa
	pha
	tsx
	lda $0102,x
	tax
	lda $1A
	jsr point_tile
	pla
	tax
	lda $1A
	bne @opaque
	lda $18
	clc
	adc $1B
	sta $18
	jmp @pair
@opaque:
	cmp #$20
	bcc :+
	clv
:
	asl a
	asl a
	asl a
	sta $1A
	tya
	pha
	ldx $1B
	lda #$00
	sta $1B
@repeat:
	lda $18
	sta $5004
	lda $19
	asl a
	asl a
	asl a
	sta $5005
	ldy $1A
	lda ($02),y
	sta $5007
	iny
	lda ($02),y
	sta $5007
	iny
	lda ($02),y
	sta $5007
	iny
	lda ($02),y
	sta $5007
	iny
	lda ($02),y
	sta $5007
	iny
	lda ($02),y
	sta $5007
	iny
	lda ($02),y
	sta $5007
	iny
	lda ($02),y
	sta $5007
	inc $18
	dex
	bne @repeat
	pla
	tay
	ldx #$FF
	jmp @pair
@end_row:
	iny
	bne :+
	inc $01
:
	jmp @row
@end:
	pla
	rts

; Clears plane-0 rows $E6-$FE, then decodes (value, count) runs on rows relative to $E6.
draw_row_runs:
	lda $15
	sta $5001
	ldy #$E6
@clear_row:
	sty $5005
	lda #$00
	sta $5004
	ldx #$20
@clear:
	sta $5007
	dex
	bne @clear
	iny
	cpy #$FF
	bne @clear_row
	ldy #$00
@offset:
	lda ($00),y
	cmp #$FE
	beq @end
	clc
	adc #$E6
	sta $5005
	lda #$00
	sta $5004
@value:
	iny
	bne :+
	inc $01
:
	lda ($00),y
	cmp #$FF
	beq @escape
	sta $1A
	iny
	bne :+
	inc $01
:
	lda ($00),y
	tax
	lda $1A
@run:
	sta $5007
	dex
	bne @run
	jmp @value
@escape:
	iny
	bne :+
	inc $01
:
	jmp @offset
@end:
	ldx #$00
	lda #$FE
	cmp #$FE
	rts

; Clears plane-0 rows $E6-$FE, then writes literal stream bytes on absolute row offsets.
draw_row_literals:
	php
	lda $15
	sta $5001
	clv
	ldy #$E6
@clear_row:
	sty $5005
	lda #$00
	sta $5004
	ldx #$20
@clear:
	sta $5007
	dex
	bne @clear
	iny
	cpy #$FF
	bne @clear_row
	ldy #$00
@offset:
	tya
	clc
	adc $00
	bcc :+
	inc $01
:
	lda ($00),y
	bcc @offset_read_done
	dec $01
@offset_read_done:
	cmp #$FE
	beq @end
	clc
	adc #$E6
	sta $5005
	lda #$00
	sta $5004
@next_index:
	iny
	bne :+
	inc $01
:
@value:
	tya
	clc
	adc $00
	bcc :+
	inc $01
:
	lda ($00),y
	bcc @value_read_done
	dec $01
@value_read_done:
	cmp #$FF
	beq @escape
	sta $5007
	jmp @next_index
@escape:
	iny
	bne :+
	inc $01
:
	jmp @offset
@end:
	php
	pla
	and #$40
	tax
	pla
	and #$1C
	ora #$23
	cpx #$40
	beq :+
	jmp @finish
:
	clc
	cld
	adc #$40
@finish:
	pha
	lda #$FE
	tax
	ldx #$00
	plp
	rts

; Selects graphics tile A from the base at $06,X; returns its low index and sets $02-$03 and $1A.
; Uses INC rather than ADC so V, D and the stored B bit are untouched; C is changed.
point_tile:
	sta $1A
	lda $06,x
	sta $02
	lda $07,x
	sta $03
	lda $1A
	asl a
	bcc :+
	inc $03
	inc $03
	inc $03
	inc $03
:
	asl a
	bcc :+
	inc $03
	inc $03
:
	asl a
	bcc :+
	inc $03
:
	rts

; Redraws enabled background records, then composites enabled objects, in record order.
composite_objects:
	pha
	txa
	pha
	tya
	pha
	php
	cld
	ldx #$00
@redraw:
	jsr copy_record
	lda $0307
	cmp #$FF
	beq @redraw_next
	txa
	pha
	ldy #$03
	cpx #$40
	bcc :+
	ldy #$02
:
	sty $1C
	jsr redraw_record
	pla
	tax
@redraw_next:
	txa
	clc
	adc #$08
	tax
	cpx #$80
	bne @redraw
	ldx #$00
@object:
	jsr copy_record
	lda $0302
	cmp #$FF
	beq @object_next
	txa
	pha
	ldy #$10
	cpx #$40
	bcc :+
	ldy #$08
:
	sty $1C
	jsr draw_object
	pla
	tax
@object_next:
	txa
	clc
	adc #$08
	tax
	cpx #$80
	bne @object
	plp
	pla
	tay
	pla
	tax
	sec
	clv
	pla
	rts

copy_record:
	ldy #$00
@byte:
	lda $21,x
	sta $0300,y
	inx
	iny
	cpy #$08
	bne @byte
	txa
	sec
	sbc #$08
	tax
	rts

; Redraws a $1C-by-$1C tile area from selectors captured in the plane-0 map rows.
redraw_record:
	lda $15
	ora #$40
	sta $5001
	lda $0305
	lsr a
	lsr a
	lsr a
	sta $1E
	lda $0306
	lsr a
	lsr a
	lsr a
	clc
	adc #$E6
	sta $1F
	lda $0306
	and #$F8
	sta $19
	ldx #$00
	stx $18
@capture_column:
	lda #$00
	sta $1B
@capture_row:
	lda $1F
	clc
	adc $1B
	sta $5005
	lda $1E
	clc
	adc $18
	sta $5004
	lda $5006
	sta $0308,x
	inx
	inc $1B
	lda $1B
	cmp $1C
	bne @capture_row
	inc $18
	lda $18
	cmp $1C
	bne @capture_column
	lda #$00
	sta $1D
@plane:
	ldy #$00
	sty $18
@column:
	lda $1E
	clc
	adc $18
	ldx $1D
	beq :+
	ora #$80
:
	sta $5004
	lda $19
	sta $5005
	lda #$00
	sta $1B
@tile:
	tya
	pha
	lda $0308,y
	ldx $1D
	jsr point_tile
	tay
	ldx #$08
@tile_row:
	lda ($02),y
	sta $5007
	iny
	dex
	bne @tile_row
	pla
	tay
	iny
	inc $1B
	lda $1B
	cmp $1C
	bne @tile
	inc $18
	lda $18
	cmp $1C
	bne @column
	lda $1D
	bne :+
	lda #$02
	sta $1D
	jmp @plane
:
	lda $1E
	ora #$80
	sta $18
	lda #$00
	sta $1B
	sta $1C
	sta $1D
	rts

; Draws a $1C-row object whose source is shifted right by X & 7 and composited over VRAM.
; Pre-draw bytes go to $308 + 48*plane + $1C*column + row; composited bytes go to $368 + ...
draw_object:
	lda $0301
	sta $19
	lda $0300
	and #$07
	sta $1B
	ldx #$00
	stx $00
@source_plane:
	lda #$00
	sta $01
@source_column:
	lda #$00
	sta $18
@source_row:
	lda $18
	lsr a
	lsr a
	lsr a
	asl a
	clc
	adc $01
	clc
	adc $0302
	jsr fetch_row
	sta $0368,x
	inx
	inc $18
	lda $18
	cmp $1C
	bne @source_row
	inc $01
	lda $1C
	lsr a
	lsr a
	lsr a
	cmp $01
	bne @source_column
	ldy $1C
	lda #$00
@zero_column:
	sta $0368,x
	inx
	dey
	bne @zero_column
	lda $00
	bne :+
	lda #$02
	sta $00
	ldx #$30
	bne @source_plane
:
	ldy $1B
	beq @shifted
@shift:
	ldx $1C
@shift_row:
	dex
	lda $1C
	cmp #$10
	bne @shift_small
	lsr $0368,x
	ror $0378,x
	ror $0388,x
	lsr $0398,x
	ror $03A8,x
	ror $03B8,x
	jmp @shift_next
@shift_small:
	lsr $0368,x
	ror $0370,x
	lsr $0398,x
	ror $03A0,x
@shift_next:
	txa
	bne @shift_row
	dey
	bne @shift
@shifted:
	lda $15
	ora #$40
	sta $5001
	ldx #$00
	stx $00
@read_plane:
	lda #$00
	sta $01
@read_column:
	lda #$00
	sta $18
@read_row:
	lda $19
	clc
	adc $18
	sta $5005
	jsr object_column
	sta $5004
	lda $5006
	sta $0308,x
	inx
	inc $18
	lda $18
	cmp $1C
	bne @read_row
	inc $01
	jsr object_columns
	cmp $01
	bne @read_column
	lda $00
	bne :+
	lda #$80
	sta $00
	ldx #$30
	bne @read_plane
:
	lda #$30
	ldy $1C
	cpy #$10
	beq :+
	lda #$10
:
	sta $18
	ldx #$00
@composite:
	lda $0368,x
	ora $0398,x
	eor #$FF
	sta $01
	and $0308,x
	ora $0368,x
	sta $0368,x
	lda $01
	and $0338,x
	ora $0398,x
	sta $0398,x
	inx
	cpx $18
	bne @composite
	lda $15
	ora #$40
	sta $5001
	lda $1C
	cmp #$10
	bne :+
	lda $15
	ora #$40
	sta $5001
:
	ldx #$00
	stx $00
@write_plane:
	lda #$00
	sta $01
@write_column:
	lda $19
	sta $5005
	jsr object_column
	sta $5004
	ldy $1C
@write_row:
	lda $0368,x
	sta $5007
	inx
	dey
	bne @write_row
	inc $01
	jsr object_columns
	cmp $01
	bne @write_column
	lda $00
	bne :+
	lda #$80
	sta $00
	ldx #$30
	bne @write_plane
:
	lda #$B0
	ldy $1C
	cpy #$10
	beq :+
	lda #$A0
:
	sta $00
	lda #$03
	sta $01
	lda $0300
	lsr a
	lsr a
	lsr a
	ora #$80
	sta $18
	lda #$01
	sta $1B
	lda #$00
	sta $1C
	rts

; Returns the raw LCD column for object column $01 in plane $00.
object_column:
	lda $0300
	lsr a
	lsr a
	lsr a
	clc
	adc $01
	ora $00
	rts

; Returns the destination column count: 3 for a large object or 2 for a small one.
object_columns:
	lda $1C
	lsr a
	lsr a
	lsr a
	sec
	adc #$00
	rts

; Reads source row $18 & 7 of tile A from the base at $06,$00; preserves X.
fetch_row:
	sta $1A
	txa
	pha
	ldx $00
	lda $1A
	jsr point_tile
	eor $18
	and #$F8
	eor $18
	tay
	lda ($02),y
	tay
	pla
	tax
	tya
	rts

fill_rectangle:
	sta $1A
	lda #$00
	beq fill_rectangle_common
fill_rectangle_plane1:
	sta $1A
	lda #$80
fill_rectangle_common:
	sta $A8
	txa
	lsr a
	lsr a
	lsr a
	ora $A8
	sta $18
	sty $19
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
	.word nmi
	.word reset
	.word irq

; Copyright (c) 2026 Ry
; Independently authored experimental Gamate firmware.
; SPDX-License-Identifier: GPL-2.0-or-later

.setcpu "6502"

; Hardware and cartridge addresses, as seen by the CPU (see docs/MEMORY_MAP.md).
AY_REGISTERS         = $4000 ; write-only; address bits 0-3 select the AY register
UART_RX_DATA         = $4800 ; completed link receive byte, read by the NMI wrapper

LCD_CONTROL          = $5001 ; bit 7 blank, 6 row increment, 5 window, 4 swap planes, 0 stop
LCD_SCROLL_X         = $5002
LCD_SCROLL_Y         = $5003
LCD_VRAM_ADDR_LO     = $5004 ; bit 7 selects the plane, bits 0-4 are VRAM address bits 0-4
LCD_VRAM_ADDR_HI     = $5005 ; VRAM address bits 5-12
LCD_VRAM_READ        = $5006 ; auto-increments the VRAM address
LCD_VRAM_WRITE       = $5007 ; auto-increments the VRAM address

CART_PROTECTION_PORT = $6000 ; bit-serial handshake; reads as cartridge ROM once unlocked
CART_CHECKSUM_LO     = $6000 ; cartridge header, ROM offset 0
CART_CHECKSUM_HI     = $6001
CART_IDENTITY        = $6005 ; "COPYRIGHT BIT CORPORATION" string
CART_ENTRY           = $6020 ; cartridge start vector
CART_NMI_CALLBACK    = $6023 ; called from the NMI wrapper while CALLBACK_ENABLE is nonzero
CART_IRQ_CALLBACK    = $6026 ; called from the IRQ wrapper while CALLBACK_ENABLE is nonzero

; Cartridge-visible low RAM and persistent BIOS state (see docs/MEMORY_MAP.md).
PROTECTION_REPLY     = $004 ; accepted serial protection reply ($47)
UART_RX_BYTE         = $00A ; completed UART byte copied by the NMI wrapper
WAIT_COUNTER         = $00B ; incremented by IRQ; consumed by the wait services
CALLBACK_ENABLE      = $00C ; zero suppresses cartridge NMI/IRQ callbacks
IRQ_COUNTER_31_24    = $00E ; big-endian 32-bit periodic-IRQ counter
IRQ_COUNTER_23_16    = $00F
IRQ_COUNTER_15_8     = $010
IRQ_COUNTER_7_0      = $011

RANDOM_STATE_LO      = $012 ; little-endian 16-bit pseudorandom state
RANDOM_STATE_HI      = $013
RANDOM_FEEDBACK      = $014 ; temporary feedback bit in bit 7

LCD_CONTROL_BASE     = $015 ; caller-owned base value used by BIOS drawing services

CHECKSUM_WORK_LO     = $0E0 ; cartridge checksum retained from successful validation
CHECKSUM_WORK_HI     = $0E1
CHECKSUM_COPY_LO     = $0E5
CHECKSUM_COPY_HI     = $0E6

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
	sta CART_PROTECTION_PORT
	dey
	bne @send_bit
	sta CART_PROTECTION_PORT
	inx
	cpx #$0F
	bne @send_byte

	lda #$00
	sta $04
	ldy #$08
@receive_bit:
	lda CART_PROTECTION_PORT
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
	lda CART_IDENTITY,x
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
	cmp CART_CHECKSUM_LO
	beq :+
	jmp bad_checksum
:
	lda $01
	cmp CART_CHECKSUM_HI
	beq :+
	jmp bad_checksum
:
	lda CART_CHECKSUM_LO
	sta CHECKSUM_WORK_LO
	sta CHECKSUM_COPY_LO
	lda CART_CHECKSUM_HI
	sta CHECKSUM_WORK_HI
	sta CHECKSUM_COPY_HI

	lda #$00
	sta CALLBACK_ENABLE
	sta WAIT_COUNTER
	ldx #$0F
@clear_ay:
	sta AY_REGISTERS,x
	dex
	bpl @clear_ay
	cli
	jsr animate_logo

	lda #$00
	sta WAIT_COUNTER
	jsr wait_24_irqs

	lda #$00
	sta LCD_CONTROL
	jsr clear_plane
	lda #$80
	jsr clear_plane

	lda #$00
	ldx #$1F
@initialize_low_ram:
	sta $0000,x
	dex
	bpl @initialize_low_ram
	sta $21
	lda #$FF
	sta $20
	lda #$00
	sta LCD_SCROLL_X
	lda #$47
	sta PROTECTION_REPLY
	lda #$FF
	ldx #$05
@initialize_sentinels:
	sta $0000,x
	inx
	cpx #$0B
	bne @initialize_sentinels
	lda #$03
	sta IRQ_COUNTER_15_8
	lda #$94
	sta IRQ_COUNTER_7_0
	lda #$8A
	sta RANDOM_STATE_LO
	lda #$35
	sta RANDOM_STATE_HI
	lda #$5F
	sta LCD_SCROLL_Y
	lda #$40
	sta LCD_CONTROL
	lda #$8E
	sta LCD_VRAM_ADDR_LO
	lda #$C6
	sta LCD_VRAM_ADDR_HI
	lda #$01
	sta CALLBACK_ENABLE
	ldx #$28
	ldy #$FF
	lda #$FF
	clv
	cld
	sec
	jmp CART_ENTRY

animate_logo:
	ldx #$24
@wait_block:
	jsr wait_24_irqs
	dex
	bne @wait_block
	jsr wait_20_irqs
	rts

clear_plane:
	sta LCD_VRAM_ADDR_LO
	lda #$00
	sta LCD_VRAM_ADDR_HI
	ldx #$20
	ldy #$00
@byte:
	sta LCD_VRAM_WRITE
	iny
	bne @byte
	dex
	bne @byte
	rts

irq:
	pha
	tya
	pha
	inc WAIT_COUNTER
	inc IRQ_COUNTER_7_0
	bne @callback
	inc IRQ_COUNTER_15_8
	bne @callback
	inc IRQ_COUNTER_23_16
	bne @callback
	inc IRQ_COUNTER_31_24
@callback:
	lda CALLBACK_ENABLE
	beq :+
	jsr CART_IRQ_CALLBACK
:
	pla
	tay
	pla
	rti

nmi:
	pha
	lda UART_RX_DATA
	sta UART_RX_BYTE
	lda CALLBACK_ENABLE
	beq :+
	jsr CART_NMI_CALLBACK
:
	pla
	rti

wait_8_irqs:
	lda WAIT_COUNTER
	cmp #$08
	bcc wait_8_irqs
	lda #$00
	sta WAIT_COUNTER
	rts

wait_24_irqs:
	lda WAIT_COUNTER
	cmp #$18
	bcc wait_24_irqs
	lda #$00
	sta WAIT_COUNTER
	rts

wait_20_irqs:
	lda WAIT_COUNTER
	cmp #$14
	bcc wait_20_irqs
	lda #$00
	sta WAIT_COUNTER
	sec
	rts

wait_counter:
	lda WAIT_COUNTER
	cmp #$05
	bcc wait_counter
	lda #$00
	sta WAIT_COUNTER
	rts

; Advances RANDOM_STATE_HI:RANDOM_STATE_LO; zero is repaired to $FFFF before the transition.
next_random:
	lda RANDOM_STATE_LO
	ora RANDOM_STATE_HI
	bne @nonzero
	lda #$FF
	sta RANDOM_STATE_LO
	sta RANDOM_STATE_HI
	lda #$00
	sta RANDOM_FEEDBACK
	lda RANDOM_STATE_HI
	and #$04
	beq @zero_finish
	lda #$80
	sta RANDOM_FEEDBACK
@zero_finish:
	.repeat 10
	nop
	.endrepeat
	bne @calculated
@nonzero:
	lda #$00
	sta RANDOM_FEEDBACK
	lda RANDOM_STATE_HI
	and #$04
	bne @bit4
	.repeat 12
	nop
	.endrepeat
	beq @calculated
@bit4:
	lda #$80
	sta RANDOM_FEEDBACK
	.repeat 9
	nop
	.endrepeat
	bne @calculated
@calculated:
	lda RANDOM_STATE_LO
	asl a
	asl a
	asl a
	eor RANDOM_FEEDBACK
	asl a
	rol RANDOM_STATE_LO
	rol RANDOM_STATE_HI
	lda RANDOM_STATE_LO
	nop                         ; match the measured uninterrupted service duration
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
	lda LCD_CONTROL_BASE
	ora #$80
	sta LCD_CONTROL
	lda #$00
	sta $1E
	jsr clear_display_plane
	lda #$80
	sta $1E
	jsr clear_display_plane
	lda LCD_CONTROL_BASE
	ora #$40
	sta LCD_CONTROL
	rts

clear_display_plane:
	ldy #$00
@row:
	lda $1E
	sta LCD_VRAM_ADDR_LO
	sty LCD_VRAM_ADDR_HI
	ldx #$20
	lda #$00
@column:
	sta LCD_VRAM_WRITE
	dex
	bne @column
	iny
	cpy #$C8
	bne @row
	rts

copy_plane:
	lda $01C
	sta $01A
@column:
	lda $018
	sta LCD_VRAM_ADDR_LO
	lda $019
	sta LCD_VRAM_ADDR_HI
	ldy #$00
	ldx $01D
@row:
	lda ($1E),y
	sta LCD_VRAM_WRITE
	sta $01B
	iny
	dex
	bne @row
	tya
	beq @full_page
	clc
	adc $1E
	sta $1E
	bcc @next_column
	inc $1F
@next_column:
	; The destination column is an eight-bit counter; INC preserves its wrap behavior while
	; avoiding redundant accumulator setup on every column of both planes.
	inc $018
	dec $01A
	bne @column
	rts
@full_page:
	inc $1F
	jmp @next_column

copy_bitmap:
	php
	txa
	pha
	lsr a
	lsr a
	lsr a
	sta $018
	sty $019
	lda LCD_CONTROL_BASE
	ora #$40
	sta LCD_CONTROL
	lda $01C
	lsr a
	lsr a
	lsr a
	sta $01C
	lda $000
	sta $1E
	lda $001
	sta $1F
	jsr copy_plane
	lda $002
	sta $000
	sta $1E
	lda $003
	sta $001
	sta $1F
	lda $018
	sec
	sbc $01C
	ora #$80
	sta $018
	jsr copy_plane
	; The copy loop advances the second-plane source by columns*rows. Its low-byte delta is the
	; documented modulo-256 Y result, including a 256-row count.
	lda $1E
	sec
	sbc $002
	tay
	pla
	tax
	pla
	and #$4C
	ora #$02
	pha
	lda $01B
	plp
	rts

clear_fixed_rectangle:
	lda LCD_CONTROL_BASE
	and #$BF
	sta LCD_CONTROL
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
	sta LCD_VRAM_ADDR_LO
	tya
	sta LCD_VRAM_ADDR_HI
	ldx #31
	lda #$00
@column:
	sta LCD_VRAM_WRITE
	dex
	bne @column
	iny
	cpy #$FF
	bne @row
	lda LCD_CONTROL_BASE
	ora #$40
	sta LCD_CONTROL
	lda #$1E
	sta LCD_VRAM_ADDR_LO
	lda #$FF
	sta LCD_VRAM_ADDR_HI
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
	lda LCD_CONTROL_BASE
	ora #$40
	sta LCD_CONTROL
	ldx #$00
@column:
	txa
	and #$03
	ora #$08
	cpx #$04
	bcc :+
	ora #$80
:
	sta LCD_VRAM_ADDR_LO
	lda #$8C
	sta LCD_VRAM_ADDR_HI
	ldy #$08
@row:
	lda #$00
	sta LCD_VRAM_WRITE
	dey
	bne @row
	inx
	cpx #$08
	bne @column
	; Match the measured 3,278-master-clock uninterrupted service duration. The independently
	; authored blank bytes above deliberately replace the unknown authentic bitmap payload.
	ldy #$85
@timing:
	dey
	bne @timing
	nop
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
	lda LCD_CONTROL_BASE
	ora #$40
	sta LCD_CONTROL
	lda $1A
	sta LCD_VRAM_ADDR_LO
	lda $1B
	sta LCD_VRAM_ADDR_HI
	lda ($00),y
	pha
	tax
	tya
	pha
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
	lda ($18),y
	sta LCD_VRAM_WRITE
	iny
	lda ($18),y
	sta LCD_VRAM_WRITE
	iny
	lda ($18),y
	sta LCD_VRAM_WRITE
	iny
	lda ($18),y
	sta LCD_VRAM_WRITE
	iny
	lda ($18),y
	sta LCD_VRAM_WRITE
	iny
	lda ($18),y
	sta LCD_VRAM_WRITE
	iny
	lda ($18),y
	sta LCD_VRAM_WRITE
	iny
	lda ($18),y
	sta LCD_VRAM_WRITE
	pla
	tax
	pla
	tay
	pla

	cmp #$21
	bcc @low_selector_flags
	cmp #$80
	bcc @low_clear_overflow
	cmp #$E1
	bcs @high_clear_overflow
	bit overflow_flag
	cmp #$81
	bcc @clear_carry
	cmp #$A1
	bcc @set_carry
	cmp #$C1
	bcc @clear_carry
	bcs @set_carry
@low_clear_overflow:
	clv
	cmp #$2F
	bcc @set_carry
	bcs @clear_carry
@high_clear_overflow:
	clv
	clc
	jmp @carry_done
@low_selector_flags:
	cmp #$20
	bcc @clear_carry
	bcs @set_carry
@clear_carry:
	clc
	jmp @carry_done
@set_carry:
	sec
@carry_done:
	inc $1A
	php
	stx $18
	tsx
	inx
	inx
	tya
	cmp $0100,x
	beq @finished
	plp
	ldx $18
	iny
	jmp @tile
@finished:
	plp
	ldx $18
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
	lda LCD_CONTROL_BASE
	ora #$40
	sta LCD_CONTROL
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
	sta LCD_VRAM_ADDR_LO
	lda $19
	asl a
	asl a
	asl a
	sta LCD_VRAM_ADDR_HI
	tya
	pha
	ldy $1A
	ldx #$08
@tile_row:
	lda ($02),y
	sta LCD_VRAM_WRITE
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
	lda LCD_CONTROL_BASE
	ora #$40
	sta LCD_CONTROL
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
	sta LCD_VRAM_ADDR_LO
	lda $19
	asl a
	asl a
	asl a
	sta LCD_VRAM_ADDR_HI
	ldy $1A
	lda ($02),y
	sta LCD_VRAM_WRITE
	iny
	lda ($02),y
	sta LCD_VRAM_WRITE
	iny
	lda ($02),y
	sta LCD_VRAM_WRITE
	iny
	lda ($02),y
	sta LCD_VRAM_WRITE
	iny
	lda ($02),y
	sta LCD_VRAM_WRITE
	iny
	lda ($02),y
	sta LCD_VRAM_WRITE
	iny
	lda ($02),y
	sta LCD_VRAM_WRITE
	iny
	lda ($02),y
	sta LCD_VRAM_WRITE
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
	lda LCD_CONTROL_BASE
	sta LCD_CONTROL
	ldy #$E6
@clear_row:
	sty LCD_VRAM_ADDR_HI
	lda #$00
	sta LCD_VRAM_ADDR_LO
	ldx #$20
@clear:
	sta LCD_VRAM_WRITE
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
	sta LCD_VRAM_ADDR_HI
	lda #$00
	sta LCD_VRAM_ADDR_LO
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
	sta LCD_VRAM_WRITE
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
	lda LCD_CONTROL_BASE
	sta LCD_CONTROL
	clv
	ldy #$E6
@clear_row:
	sty LCD_VRAM_ADDR_HI
	lda #$00
	sta LCD_VRAM_ADDR_LO
	ldx #$20
@clear:
	sta LCD_VRAM_WRITE
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
	sta LCD_VRAM_ADDR_HI
	lda #$00
	sta LCD_VRAM_ADDR_LO
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
	sta LCD_VRAM_WRITE
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

.segment "COMPOSITOR"

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
	lda $28,x
	cmp #$FF
	beq @redraw_next
	lda $26,x
	sta $0305
	lda $27,x
	sta $0306
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
	cpx #$80
	bne @object
	; The optimized two-pass scan is 22 CPU cycles shorter than the measured empty-table path.
	; Keep the fixed 6,326-master-clock baseline without adding work to any per-record operation.
	.repeat 11
	nop
	.endrepeat
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
	rts

; Redraws a $1C-by-$1C tile area from selectors captured in the plane-0 map rows.
redraw_record:
	lda LCD_CONTROL_BASE
	ora #$40
	sta LCD_CONTROL
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
	sta LCD_VRAM_ADDR_HI
	lda $1E
	clc
	adc $18
	sta LCD_VRAM_ADDR_LO
	lda LCD_VRAM_READ
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
	sta LCD_VRAM_ADDR_LO
	lda $19
	sta LCD_VRAM_ADDR_HI
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
	sta LCD_VRAM_WRITE
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
	; Match the resolved per-operation timing after the direct disabled-record scan. The two sizes
	; have distinct measured fixed costs; these loops do not alter compositor-visible memory.
	lda $1C
	cmp #$03
	beq @large_timing
	ldy #$1C
@small_timing_loop:
	dey
	bne @small_timing_loop
	.repeat 3
	nop
	.endrepeat
	jmp @timing_done
@large_timing:
	ldy #$5C
@large_timing_loop:
	dey
	bne @large_timing_loop
	.repeat 4
	nop
	.endrepeat
@timing_done:
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
	lda $1C
	cmp #$08
	bne @generic_source
	jsr prepare_small_object_source
	jmp @source_ready
@generic_source:
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
	and #$07
	bne @source_pointer_ready
	lda $18
	lsr a
	lsr a
	lsr a
	asl a
	clc
	adc $01
	clc
	adc $0302
	sta $1A
	txa
	pha
	ldx $00
	lda $1A
	jsr point_object_tile
	tay
	pla
	tax
@source_pointer_ready:
	lda ($02),y
	sta $0368,x
	iny
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
@source_ready:
	ldy $1B
	bne @shift_nonzero
	lda $1C
	cmp #$10
	beq @large_zero_shift
	jmp @shifted
@large_zero_shift:
	; The paired-row loop below removes 32 CPU cycles per large-object shift. A zero-shift
	; large object bypasses it, so retain its separately measured fixed cost here.
	ldx #$06
@large_zero_timing:
	dex
	bne @large_zero_timing
	.repeat 4
	nop
	.endrepeat
	jmp @shifted
@shift_nonzero:
	lda $1C
	cmp #$10
	beq @shift_large
@shift_small:
	.repeat 8, row
	lsr $0368 + row
	ror $0370 + row
	lsr $0398 + row
	ror $03A0 + row
	.endrepeat
	; The specialized source loader removes the generic traversal's excess fixed work. Restore the
	; authentic 72-cycle cost for each applied horizontal shift without changing object state.
	ldx #$0D
@small_shift_timing:
	dex
	bne @small_shift_timing
	.repeat 3
	nop
	.endrepeat
	dey
	bne @shift_small
	beq @shifted
@shift_large:
	ldx #$10
@shift_large_row:
	dex
	lsr $0368,x
	ror $0378,x
	ror $0388,x
	lsr $0398,x
	ror $03A8,x
	ror $03B8,x
	dex
	lsr $0368,x
	ror $0378,x
	ror $0388,x
	lsr $0398,x
	ror $03A8,x
	ror $03B8,x
	txa
	bne @shift_large_row
	; Pairing rows saves 40 CPU cycles per shift; four NOPs retain the measured net 32-cycle
	; reduction. The remaining loop supplies the measured fixed nonzero-shift cost.
	.repeat 4
	nop
	.endrepeat
	dey
	bne @shift_large
	ldx #$08
@large_shift_timing:
	dex
	bne @large_shift_timing
	nop
@shifted:
	lda LCD_CONTROL_BASE
	ora #$40
	sta LCD_CONTROL
	ldx #$00
	stx $00
@read_plane:
	lda #$00
	sta $01
@read_column:
	lda $0300
	lsr a
	lsr a
	lsr a
	clc
	adc $01
	ora $00
	sta $02
	ldy $19
	lda $1C
	sta $18
@read_row:
	sty LCD_VRAM_ADDR_HI
	lda $02
	sta LCD_VRAM_ADDR_LO
	lda LCD_VRAM_READ
	sta $0308,x
	inx
	iny
	dec $18
	bne @read_row
	inc $01
	lda $1C
	lsr a
	lsr a
	lsr a
	sec
	adc #$00
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
	lda LCD_CONTROL_BASE
	ora #$40
	sta LCD_CONTROL
	lda $1C
	cmp #$10
	bne :+
	lda LCD_CONTROL_BASE
	ora #$40
	sta LCD_CONTROL
:
	ldx #$00
	stx $00
@write_plane:
	lda #$00
	sta $01
@write_column:
	lda $19
	sta LCD_VRAM_ADDR_HI
	lda $0300
	lsr a
	lsr a
	lsr a
	clc
	adc $01
	ora $00
	sta LCD_VRAM_ADDR_LO
	ldy $1C
@write_row:
	lda $0368,x
	sta LCD_VRAM_WRITE
	inx
	dey
	bne @write_row
	inc $01
	lda $1C
	lsr a
	lsr a
	lsr a
	sec
	adc #$00
	cmp $01
	bne @write_column
	lda $00
	bne :+
	lda #$80
	sta $00
	ldx #$30
	bne @write_plane
:
	ldy $1C
	lda #$A0
	cpy #$10
	bne :+
	lda #$B0
	; The optimized large-object path is 184 CPU cycles shorter than the resolved contract.
	ldy #$22
@large_object_timing:
	dey
	bne @large_object_timing
	.repeat 4
	nop
	.endrepeat
	nop
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
	lda $08
	sta $02
	rts

; The eight-pixel object needs one eight-row source tile and one cleared spill column per plane.
; Keep this fixed-size path separate from the large object's variable two-tile traversal.
prepare_small_object_source:
	lda $0302
	ldx #$00
	jsr point_object_tile
	tay
	ldx #$00
@plane0_row:
	lda ($02),y
	sta $0368,x
	lda #$00
	sta $0370,x
	iny
	inx
	cpx #$08
	bne @plane0_row
	lda $0302
	ldx #$02
	jsr point_object_tile
	tay
	ldx #$00
@plane1_row:
	lda ($02),y
	sta $0398,x
	lda #$00
	sta $03A0,x
	iny
	inx
	cpx #$08
	bne @plane1_row
	; Fixed 255-cycle component of the authentic small-object path.
	ldx #$2F
@small_fixed_timing:
	dex
	bne @small_fixed_timing
	.repeat 3
	nop
	.endrepeat
	rts

; Object fetches expose the authentic selector-dependent timing directly. Redraw traversal still
; uses point_tile because its current fixed traversal cost is calibrated together with that lookup;
; the two paths must remain separate until redraw is rewritten as a unit.
point_object_tile:
	cpx #$00
	beq :+
	jmp point_tile
:
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
	ldx #$11
@selector_bit7_timing:
	dex
	bne @selector_bit7_timing
	nop
	nop
:
	asl a
	bcc :+
	inc $03
	inc $03
	ldx #$09
@selector_bit6_timing:
	dex
	bne @selector_bit6_timing
:
	asl a
	bcc :+
	inc $03
	ldx #$03
@selector_bit5_timing:
	dex
	bne @selector_bit5_timing
	.repeat 4
	nop
	.endrepeat
:
	rts

.segment "CODE"

fill_rectangle:
	; The authentic plane-0 entry is one CPU cycle longer than the plane-1 entry. Force absolute
	; addressing for this otherwise-zero-page scratch write to preserve that measured difference.
	sta a:$001A
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
	lda LCD_CONTROL_BASE
	ora #$40
	sta LCD_CONTROL
	lda $1C
	lsr a
	lsr a
	lsr a
	tax
@column:
	lda $18
	sta LCD_VRAM_ADDR_LO
	lda $19
	sta LCD_VRAM_ADDR_HI
	ldy $1D
	lda $1A
@row:
	sta LCD_VRAM_WRITE
	dey
	bne @row
	inc $18
	dex
	bne @column
	clc
	ldx #$00
	ldy #$00
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

/*
 *  Uzebox Kernel
 *  Copyright (C) 2008-2009 Alec Bourque
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Uzebox is a reserved trade mark
*/

/*
 * NULL mixer
 *
 */

#include <avr/io.h>
#include <defines.h>

.global update_sound_buffer
.global update_sound_buffer_2
.global update_sound_buffer_fast
.global process_music
.global sound_enabled
.global update_sound

//Public variables
.global mixer

.section .bss


sound_enabled:.space 1

.section .text

;**********************
; Mix sound and process music track
; NOTE: registers r18-r27 are already saved by the caller
;***********************
process_music:
	ret


;**********************************
; Optimized sound output (cannot be called during double rate sync)
; NO MIDI
; Destroys: Z,r16,r17
; Cycles: 24
;**********************************
update_sound_buffer_fast:
	rjmp . ;lds ZL,mix_pos
	rjmp . ;lds ZH,mix_pos+1
			
	rjmp . ;ld r16,Z+		;load next sample
	nop
	rjmp . ;sts _SFR_MEM_ADDR(OCR2A),r16 ;output sound byte

	;compare+wrap=8 cycles fixed
	nop; ldi r16,hi8(MIX_BUF_SIZE+mix_buf)
	nop;cpi ZL,lo8(MIX_BUF_SIZE+mix_buf)
	nop;cpc ZH,r16
	;12

	nop;ldi r16,lo8(mix_buf)
	nop;ldi r17,hi8(mix_buf)
	nop;brlo .+2
	nop;movw ZL,r16

	rjmp .;sts mix_pos,ZL
	rjmp .;sts mix_pos+1,ZH		

	ret ;20+4=24


;************************
; NULL mixer version.
; Keeps video sync edges on the same cycles as the original.
; In: ZL = video phase (1=pre-eq/post-eq, 2=hsync)
; Destroys: ZH
; Cycles: VSYNC = 68
;         HSYNC = 135
;***********************
update_sound:
	push	r16
	push	r17
	push	r18
	push	ZL

	; 43-cycle dummy body:
	; 	ldi		= 1
	;	14x dec		= 14
	;	13x taken brne	= 26
	;	final brne	= 1
	;	nop		= 1
	;	total		= 43
	ldi		ZH,14
.Lnull_delay:
	dec		ZH
	brne	.Lnull_delay
	nop

	pop		ZL
	pop		r18
	pop		r17
	pop		r16

	;*** Video sync update ***
	sbrc	ZL,0								;pre-eq/post-eq sync
	sbi		_SFR_IO_ADDR(SYNC_PORT),SYNC_PIN	;TCNT1=0xAC
	sbrs	ZL,0
	rjmp	.+2
	ret

	ldi		ZH,20
	dec		ZH
	brne	.-4
	rjmp	.

	;*** Video sync update ***
	sbrc	ZL,1								;hsync
	sbi		_SFR_IO_ADDR(SYNC_PORT),SYNC_PIN	;TCNT1=0xF0
	sbrs	ZL,1
	rjmp	.

	ret
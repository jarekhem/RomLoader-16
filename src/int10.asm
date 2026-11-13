CPU 8086
BITS 16

%include "font00-7F.inc"

biosDataSeg 	equ 0x0040


video_mode	    equ	49h	; byte - active video mode number
video_columns	equ	4Ah	; word - number of text columns for active mode
video_page_size	equ	4Ch	; word - size of video page in bytes
video_page_offt	equ	4Eh	; word - offset of the active video page
video_cur_pos	equ	50h	; byte[16] - cursor position for each page
video_cur_shape	equ	60h	; word - cursor shape
video_page	    equ	62h	; byte - active video page
video_port      equ	63h	; word - I/O port for the display adapter
video_mode_reg	equ	65h	; byte - video adapter mode register
video_palet_reg	equ	66h	; byte - color palette


global int_10
global int_1D

;=========================================================================
; video.inc - BIOS video services
;       INT 10h, functions AH=00h to AH=0Fh
;-------------------------------------------------------------------------

; CRTC registers
crtc_cur_start	equ	0Ah		; CRTC cursor start line register
crtc_cur_end	equ	0Bh		; CRTC cursor end line register
crtc_offset_hi	equ	0Ch		; CRTC start address high register
crtc_offset_lo	equ	0Dh		; CRTC start address low register
crtc_cur_pos_hi	equ	0Eh		; CRCT cursor location high register
crtc_cur_pos_lo	equ	0Fh		; CRTC cursor location low register
crtc_pen_hi	    equ	10h		; CRTC light pen position high byte
crtc_pen_lo	    equ	11h		; CRTC light pen position low byte

; control characters
bel	equ	07h
bs	equ	08h
lf	equ	0Ah
cr	equ	0Dh

int_10_dispatch:
	dw	int_10_fn00		; Set video mode
	dw	int_10_fn01		; Set text mode cursor shape
	dw	int_10_fn02		; Set cursor position
	dw	int_10_fn03		; Get cursor position and shape
	dw	int_10_fn04		; Read light pen position
	dw	int_10_fn05		; Set active display page
	dw	int_10_fn06		; Scroll up window
	dw	int_10_fn07		; Scroll down window
	dw	int_10_fn08		; Read character and attribute
	dw	int_10_fn09		; Write character and attribute
	dw	int_10_fn0A		; Write character only
	dw	int_10_fn0B		; Set background color or palette
	dw	int_10_fn0C		; Write graphics pixel
	dw	int_10_fn0D		; Read graphics pixel
	dw	int_10_fn0E		; Teletype output
	dw	int_10_fn0F		; Get current video mode
int_10_num_func	equ ($-int_10_dispatch)/2

int_10_ax	equ	0
int_10_al	equ	int_10_ax
int_10_ah	equ	int_10_ax+1
int_10_bx	equ	int_10_ax+2
int_10_bl	equ	int_10_bx
int_10_bh	equ	int_10_bx+1
int_10_cx	equ	int_10_bx+2
int_10_ch	equ	int_10_cx+1
int_10_dx	equ	int_10_cx+2

SECTION .text

int_10:
	sti
	cld
	push	bp
	push	es
	push	ds
	push	si
	push	di
	push	dx
	push	cx
	push	bx
	push	ax
	mov	bx,biosDataSeg
	mov	ds,bx
	mov	bl,byte [video_mode]
	cmp	bl,07h
	mov	bx,0B800h
	jb	.color
	mov	bh,0B0h

.color:
	mov	es,bx
	mov	bp,sp
	cmp	ah,int_10_num_func
	jae	.exit
	mov	bh,0
	mov	bl,ah
	shl	bx,1
    cs	call	near [int_10_dispatch+bx]

.exit:
	pop	ax
	pop	bx
	pop	cx
	pop	dx
	pop	di
	pop	si
	pop	ds
	pop	es
	pop	bp
	iret

int_1D:
	; CGA text 40x25 modes
	db	38h, 28h, 2Dh, 0Ah, 1Fh, 06h, 19h, 1Ch
	db	02h, 07h, 06h, 07h, 00h, 00h, 00h, 00h

	; CGA text 80x25 modes
	db	71h, 50h, 5Ah, 0Ah, 1Fh, 06h, 19h, 1Ch
	db	02h, 07h, 06h, 07h, 00h, 00h, 00h, 00h

	; CGA graphics modes
	db	38h, 28h, 2Dh, 0Ah, 7Fh, 06h, 64h, 70h
	db	02h, 01h, 06h, 07h, 00h, 00h, 00h, 00h

	; MDA text 80x25 mode
	db	61h, 50h, 52h, 0Fh, 19h, 06h, 19h, 19h
	db	02h, 0Dh, 0Bh, 0Ch, 00h, 00h, 00h, 00h

page_size:
	dw	0800h
	dw	1000h
	dw	4000h
	dw	4000h

columns:
	db	40, 40
	db	80, 80
	db	40, 40
	db	80, 80

MODES	db	2Ch,28h,2Dh,29h,2Ah,2Eh,1Eh,29h
TABMUL	db	00h,00h,10h,10h,20h,20h,20h,30h

int_10_fn00:
	mov	bl,byte [bp+int_10_al]
	mov	cx,0B800h
	mov	dx,3D4h
	mov	al,0
	cmp	bl,07h
	jb	.color
	mov	ch,0B0h
	mov	dl,0B4h
	inc	al

.color:
	mov	es,cx
	mov	word [video_port],dx
	add	dl,4
	out	dx,al
	mov	byte [video_mode],bl
	mov	bh,0
	push	bx
	push	es
	xor	ax,ax
	mov	es,ax
    es	les	si,[1Dh*4]
    cs	mov	bl,byte [bx+TABMUL]
	add	si,bx

    es	mov	cx,word [si+crtc_cur_start]
	xchg	cl,ch
	mov	word [video_cur_shape],cx

	mov	cx,10h

.setup_crt_loop:
    es	mov	al,byte [si]
	call	vid_crtc_writeb
	inc	ah
	inc	si
	loop	.setup_crt_loop
	pop	es
	xor	di,di
	mov	cx,2000h
	xor	ax,ax
	call	vid_check_mode
	jc	.clear_screen
	jnz	.text_fill
	mov	cx,0800h

.text_fill:
	mov	ax,07h << 8 | ' '

.clear_screen:
	repz	stosw

	mov	dx,word [video_port]
	add	dl,4
	pop	bx
    cs	mov	al,byte [bx+MODES]
	out	dx,al
	mov	byte [video_mode_reg],al
	inc	dx
	mov	al,30h
	cmp	bl,6
	jnz	.set_palette
	mov	al,3Fh

.set_palette:
	mov	byte [video_palet_reg],al
	out	dx,al

	mov	ax,ds
	mov	es,ax
	xor	ax,ax
	mov	byte [video_page],al
	mov	cx,9
	mov	di,video_page_offt
 	rep	stosw

    cs	mov	al,byte [bx+columns]
	mov	word [video_columns],ax
	and	bl,0FEh
    cs	mov	ax,word [bx+page_size]
	mov	word [video_page_size],ax
	ret

int_10_fn01:
	mov	word [video_cur_shape],cx
	mov	ah,crtc_cur_start
	call	vid_crtc_writew
	ret

int_10_fn02:
	mov	bl,byte [bp+int_10_bh]
	cmp	byte [video_page],bl
	jne	bios_set_cur_pos

set_cur_pos:
	mov	ax,dx
	call	vid_position_to_offset
	add	ax,word [video_page_offt]
	shr	ax,1
	mov	cx,ax
	mov	ah,crtc_cur_pos_hi
	call	vid_crtc_writew
bios_set_cur_pos:
	mov	bh,0
	shl	bl,1
	mov	word [bx+video_cur_pos],dx
	ret

int_10_fn03:
	mov	bh,0
	mov	bl,byte [bp+int_10_bh]
	shl	bl,1
	mov	ax,word [bx+video_cur_pos]
	mov	word [bp+int_10_dx],ax
	mov	ax,word [video_cur_shape]
	mov	word [bp+int_10_cx],ax
	ret

int_10_fn04:
	mov	byte [bp+int_10_ah],0
	mov	dx,word [video_port]
	add	dl,6
	in	al,dx
	test	al,4
	jz	.reset_pen
	test	al,2
	jnz	.read_pen
	ret

.read_pen:
	mov	dx,word [video_port]
	mov	al,crtc_pen_hi
	out	dx,al
	inc	dx
	in	al,dx
	mov	ah,al
	dec	dx
	mov	al,crtc_pen_lo
	out	dx,al
	inc	dx
	in	al,dx

	mov	bh,0
	mov	bl,byte [video_mode]
    cs	mov	bl,byte [bx+.correction]
	sub	ax,bx
	jns	.1
	xor	ax,ax

.1:
	call	vid_check_mode
	jnc	.text
	mov	dl,40
	div	dl
	mov	bh,0
	mov	bl,ah
	mov	cl,3
	shl	bx,cl
	mov	ch,al
	shl	ch,1
	mov	dl,ah
	mov	dh,al
	shr	dh,1
	shr	dh,1
	cmp	byte [video_mode],6
	jnz	.exit
	shl	bx,1
	shl	dl,1
	jmp	.exit

.text:
	div	byte [video_columns]
	xchg	al,ah
	mov	dx,ax
	mov	cl,3
	shl	ah,cl
	mov	ch,ah
	mov	bh,0
	mov	bl,al
	shl	bx,cl

.exit:
	mov	byte [bp+int_10_ah],1
	mov	word [bp+int_10_dx],dx
	mov	word [bp+int_10_bx],bx
	mov	byte [bp+int_10_ch],ch

.reset_pen:
	mov	dx,word [video_port]
	add	dl,7
	out	dx,al
	ret

.correction:
	db	3, 3, 5, 5, 3, 3, 3, 4

int_10_fn05:
	mov	byte [video_page],al
	mov	bl,al
	mov	ah,0
	mul	word [video_page_size]
	mov	word [video_page_offt],ax
	shr	ax,1
	mov	cx,ax
	mov	ah,crtc_offset_hi
	call	vid_crtc_writew

	mov	bh,0
	shl	bx,1
	mov	ax,word [bx+video_cur_pos]
	call	vid_position_to_offset
	shr	ax,1
	add	cx,ax
	mov	ah,crtc_cur_pos_hi
	call	vid_crtc_writew
	ret

int_10_fn06:
int_10_fn07:
	call	vid_check_mode
	jc	.graphics_scroll

	xor	si,si
	cmp	byte [video_mode],2
	jb	.no_snow
	cmp	byte [video_mode],3
	ja	.no_snow
	mov	si,0101010101010101b
.no_snow:
	mov	ax,word [bp+int_10_dx]
	push	ax
	cmp	byte [bp+int_10_ah],07h
	jz	.1
	mov	ax,word [bp+int_10_cx]

.1:
	call	vid_position_to_offset
	add	ax,word [video_page_offt]
	mov	di,ax

	pop	dx
	sub	dx,word [bp+int_10_cx]
	add	dx,101h

	mov	bx,word [video_columns]
	shl	bx,1
	mov	al,byte [bp+int_10_al]
	push    dx
	mov     ah,0
	mul     bx
	pop     dx

	sub	bl,dl
	sub	bl,dl
	push	ds
	mov	cx,es
	mov	ds,cx
	cmp	byte [bp+int_10_ah],06h
	jz	.2
	neg	ax
	neg	bx
	std

.2:
	mov	cl,byte [bp+int_10_al]
	or	cl,cl
	jz	.text_fill_only
	xchg	ax,si
	add	si,di
	sub	dh,cl

	or	bx,bx
	jz	.text_full_row_scroll

.text_scroll_loop:
	mov	ch,0
	mov	cl,dl

	ror	ax,1
	jnc	.text_scroll_no_retrace
	call	.retrace_wait

.text_scroll_no_retrace:
	repz	movsw

	add	si,bx
	add	di,bx
	dec	dh
	jnz	.text_scroll_loop

.text_fill:
	mov	dh,byte [bp+int_10_al]
	mov	si,ax

.text_fill_only:
	mov	ch,0
	mov	ah,byte [bp+int_10_bh]
	mov	al,' '

.text_fill_loop:
	mov	cl,dl
	ror	si,1
	jnc	.text_fill_no_retrace
	call	.retrace_wait

.text_fill_no_retrace:
	repz	stosw
	add	di,bx
	dec	dh
	jnz	.text_fill_loop

	pop	ds
	ret

.text_full_row_scroll:
	or	ax,ax
	jz	.text_full_row_no_snow
	push	ax
	mov	al,dl
	mul	dh

.text_full_row_loop:
	mov	cx,240
	cmp	ax,cx
	ja	.copy_chunk
	xchg	ax,cx
	xor	ax,ax
	jmp	.do_copy

.copy_chunk:
	sub	ax,cx

.do_copy:
	call	.retrace_wait
	rep	movsw
	or	ax,ax
	jnz	.text_full_row_loop
	pop	ax
	jmp	.text_fill

.text_full_row_no_snow:
	push	ax
	mov	al,dl
	mul	dh
	mov	cx,ax
	rep	movsw
	pop	ax
	jmp	.text_fill

.retrace_wait:
	push	ax
	push	dx
	mov	dx,03DAh

.retrace_wait_not_set:
	in	al,dx
	test	al,08h
	jnz	.retrace_wait_not_set

.retrace_wait_set:
	in	al,dx
	test	al,08h
	jz	.retrace_wait_set
	pop	dx
	pop	ax

.retrace_exit:
	ret

.graphics_scroll:
	mov	ax,word [bp+int_10_dx]
	push	ax
	cmp	byte [bp+int_10_ah],07h
	jz	.3
	mov	ax,word [bp+int_10_cx]

.3:
	call	vid_gfx_pos_to_offset
	mov	di,ax

	pop	dx
	sub	dx,word [bp+int_10_cx]
	add	dx,101h
	shl	dh,1
	shl	dh,1
	mov	al,byte [bp+int_10_ah]
	cmp	byte [video_mode],06h
	jz	.4
	shl	dl,1
	shl	di,1
	cmp	al,07h
	jnz	.5
	inc	di

.4:
	cmp	al,07h
	jnz	.5
	add	di,0F0h

.5:
	mov	bl,byte [bp+int_10_al]
	shl	bl,1
	shl	bl,1
	push	bx
	sub	dh,bl
	mov	al,50h
	mul	bl
	mov	bx,1FB0h
	cmp	byte [bp+int_10_ah],06h
	jz	.6
	neg	ax
	mov	bx,2050h
	std

.6:
	mov	si,di
	add	si,ax
	pop	ax
	mov	cx,es
	mov	ds,cx
	or	al,al
	jz	.graphics_fill
	push	ax

.graphics_scroll_loop:
	mov	ch,0
	mov	cl,dl
	push	si
	push	di
	repz	movsb
	pop	di
	pop	si
	add	si,2000h
	add	di,2000h
	mov	cl,dl
	push	si
	push	di
	repz	movsb
	pop	di
	pop	si
	sub	si,bx
	sub	di,bx
	dec	dh
	jnz	.graphics_scroll_loop

	pop	ax
	mov	dh,al

.graphics_fill:
	mov	al,byte [bp+int_10_bh]
	mov	ch,0

.graphics_fill_loop:
	mov	cl,dl
	push	di
	repz	stosb
	pop	di
	add	di,2000h
	mov	cl,dl
	push	di
	repz	stosb
	pop	di
	sub	di,bx
	dec	dh
	jnz	.graphics_fill_loop
	ret

int_10_fn08:
int_10_fn09:
int_10_fn0A:
	call	vid_check_mode
	jc	.graphics
	mov	bl,byte [bp+int_10_bh]
	mov	bh,0
	push	bx
	call	vid_current_offset
	mov	di,ax
	pop	ax
	mul	word [video_page_size]
	add	di,ax
	mov	si,di
	mov	dx,word [video_port]
	add	dx,6
	push	ds
	mov	bx,es
	mov	ds,bx
	mov	al,byte [bp+int_10_ah]
	cmp	al,08h
	jnz	.text_write

.read_retrace_wait:
	in	al,dx
	test	al,01h
	jnz	.read_retrace_wait
	cli

.read_no_retrace_wait:
	in	al,dx
	test	al,01h
	jz	.read_no_retrace_wait

	lodsw
	sti
	pop	ds
	mov	word [bp+int_10_ax],ax
	ret

.text_write:
	mov	bl,byte [bp+int_10_al]
	mov	bh,byte [bp+int_10_bl]
	mov	cx,word [bp+int_10_cx]
	cmp	al,0Ah
	jz	.text_write_char_only

.write_char_retrace:
	in	al,dx
	test	al,08h
	jnz	.do_write_char_attr

.write_retrace_wait1:
	in	al,dx
	test	al,01h
	jnz	.write_retrace_wait1
	cli

.write_no_retrace_wait1:
	in	al,dx
	test	al,01h
	jz	.write_no_retrace_wait1

.do_write_char_attr:
	mov	ax,bx
	stosw
	sti
	loop	.write_char_retrace
	pop	ds
	ret

.text_write_char_only:
	in	al,dx
	test	al,08h
	jnz	.do_write_char_only

.write_retrace_wait2:
	in	al,dx
	test	al,01h
	jnz	.write_retrace_wait2
	cli

.write_no_retrace_wait2:
	in	al,dx
	test	al,01h
	jz	.write_no_retrace_wait2

.do_write_char_only:
	mov	al,bl
	stosb
	sti
	inc	di
	loop	.text_write_char_only
	pop	ds
	ret

.graphics:
	cmp	byte [bp+int_10_ah],08h
	jz	.graphics_read

	mov	ax,word [video_cur_pos]
	call	vid_gfx_pos_to_offset
	mov	di,ax
	push	ds
	mov	al,byte [bp+int_10_al]
	mov	ah,0
	or	al,al
	js	.CG9_02
	mov	dx,cs
	mov	si,gfx_font
	jmp	.CG9_03

.CG9_02:
	and	al,7Fh
	xor	bx,bx
	mov	ds,bx
	lds	si,[7Ch]
	mov	dx,ds

.CG9_03:
	pop	ds
	mov	cl,3
	shl	ax,cl
	add	si,ax
	mov	cx,word [bp+int_10_cx]
	cmp	byte [video_mode],6
	push	ds
	mov	ds,dx
	jz	.CG8_02
	shl	di,1
	mov	al,byte [bp+int_10_bl]
	and	ax,3
	mov	bx,5555h
	mul	bx
	mov	dx,ax
	mov	bl,byte [bp+int_10_bl]

.CG9_04:
	mov	bh,8
	push	di
	push	si

.CG9_05:
	lodsb
	push	cx
	push	bx
	xor	bx,bx
	mov	cx,8

.CG9_06:
	shr	al,1
	rcr	bx,1
	sar	bx,1
	loop	.CG9_06

	mov	ax,bx
	pop	bx
	pop	cx
	and	ax,dx
	xchg	ah,al
	or	bl,bl
	jns	.CG9_07
    es	xor	ax,word [di]

.CG9_07:
    es	mov	word [di],ax
	xor	di,2000h
	test	di,2000h
	jnz	.CG9_08
	add	di,50h

.CG9_08:
	dec	bh
	jnz	.CG9_05
	pop	si
	pop	di
	inc	di
	inc	di
	loop	.CG9_04
	pop	ds
	ret

.CG8_02:
	mov	bl,byte [bp+int_10_bl]
	mov	dx,2000h

.CG8_03:
	mov	bh,8
	push	di
	push	si

.CG8_04:
	lodsb
	or	bl,bl
	jns	.CG8_05
    es	xor	al,byte [di]

.CG8_05:
    es	mov	byte [di],al
	xor	di,dx
	test	di,dx
	jnz	.CG8_06
	add	di,50h

.CG8_06:
	dec	bh
	jnz	.CG8_04
	pop	si
	pop	di
	inc	di
	loop	.CG8_03
	pop	ds
	ret

.graphics_read:
	cld
	mov	ax,word [video_cur_pos]
	call	vid_gfx_pos_to_offset
	mov	si,ax
	sub	sp,8
	mov	di,sp
	cmp	byte [video_mode],6
	mov	ax,es
	push	ds
	push	di
	mov	ds,ax
	jz	CGR_06
	mov	dh,8
	shl	si,1
	mov	bx,2000h

CGR_02:
	mov	ax,word [si]
	xchg	ah,al
	mov	cx,0C000h
	mov	dl,0

CGR_03:
	test	ax,cx
	clc
	jz	CGR_04
	stc

CGR_04:
	rcl	dl,1
	shr	cx,1
	shr	cx,1
	jnb	CGR_03
    ss	mov	byte [di],dl
	inc	di
	xor	si,bx
	test	si,bx
	jnz	CGR_05
	add	si,50h

CGR_05:
	dec	dh
	jnz	CGR_02
	jmp	short	CGR_08

CGR_06:
	mov	dh,4

CGR_07:
	mov	ah,byte [si]
    ss	mov	byte [di],ah
	inc	di
	mov	ah,byte [si+2000h]
    ss	mov	byte [di],ah
	inc	di
	add	si,50h
	dec	dh
	jnz	CGR_07

CGR_08:
	mov	dx,cs
	mov	di,gfx_font
	mov	es,dx
	mov	dx,ss
	mov	ds,dx
	pop	si
	mov	al,0

CGR_09:
	mov	dx,80h

CGR_10:
	push	si
	push	di
	mov	cx,8
	repz	cmpsb
	pop	di
	pop	si
	jz	CGR_11
	inc	al
	add	di,8
	dec	dx
	jnz	CGR_10

	or	al,al
	jz	CGR_11
	xor	bx,bx
	mov	ds,bx
	les	di,[1Fh*4]
	mov	bx,es
	or	bx,di
	jz	CGR_11
	jmp	short	CGR_09

CGR_11:
	mov	byte [bp+int_10_al],al
	pop	ds
	add	sp,8
	ret

int_10_fn0B:
	mov	al,byte [video_palet_reg]
	mov	ah,byte [bp+int_10_bl]
	cmp	byte [bp+int_10_bh],00h
	jnz	.set_palette

	and	al,0E0h
	and	ah,1Fh
	or	al,ah
	jmp	.write_palet_reg

.set_palette:
	and	al,0DFh
	test	ah,01h
	jz	.write_palet_reg
	or	al,20h

.write_palet_reg:
	mov	byte [video_palet_reg],al
	mov	dx,word [video_port]
	add	dx,5
	out	dx,al
	ret

int_10_fn0C:
	call	vid_pixel_address
	jnz	.mode_320x200
	mov	al,byte [bp+int_10_al]
	mov	bl,al
	and	al,1
	ror	al,1
	mov	ah,7Fh
	jmp	.prepare_mask

.mode_320x200:
	shl	cl,1
	mov	al,byte [bp+int_10_al]
	mov	bl,al
	and	al,3
	ror	al,1
	ror	al,1
	mov	ah,3Fh

.prepare_mask:
	ror	ah,cl
	shr	al,cl
    es	mov	cl,byte [si]
	or	bl,bl
	jns	.set_color
	xor	cl,al
	jmp	.write_pixel

.set_color:
	and	cl,ah
	or	cl,al

.write_pixel:
    es	mov	[si],cl
	ret

int_10_fn0D:
	call	vid_pixel_address
    es	mov	al,byte [si]
	jnz	.mode_320x200
	shl	al,cl
	rol	al,1
	and	al,1
	jmp	.exit

.mode_320x200:
	shl	cl,1
	shl	al,cl
	rol	al,1
	rol	al,1
	and	al,3

.exit:
	mov	byte [bp+int_10_al],al
	ret

int_10_fn0E:
	mov	bl,byte [video_page]
	mov	bh,0
	shl	bl,1
	mov	dx,word [bx+video_cur_pos]

	mov	al,byte [bp+int_10_al]
	cmp	al,bs
	jz	.bs
	cmp	al,lf
	jz	.lf
	cmp	al,bel
	jz	.bel
	cmp	al,cr
	jz	.cr
	mov	bl,byte [bp+int_10_bl]
	mov	ah,0Ah
	mov	cx,1
	int	10h
	inc	dl
	cmp	dl,byte [video_columns]
	jnz	.set_cursor_pos
	mov	dl,0

.lf:
	cmp	dh,24
	jz	.scroll
	inc	dh
	jnz	.set_cursor_pos

.bs:
	cmp	dl,0
	jz	.set_cursor_pos
	dec	dl
	jmp	.set_cursor_pos

.cr:
	mov	dl,0

.set_cursor_pos:
	mov	bl,byte [video_page]
	jmp	set_cur_pos

.bel:
	mov	bl,2
	ret

.scroll:
	mov	ah,02h
	int	10h
	call	vid_check_mode
	mov	bh,0
	jc	.do_scroll
	mov	ah,08h
	int	10h
	mov	bh,ah

.do_scroll:
	mov	ah,06h
	mov	al,1
	xor	cx,cx
	mov	dh,24
	mov	dl,byte [video_columns]
	dec	dl
	int	10h
	ret

int_10_fn0F:
	mov	al,byte [video_columns]
	mov	byte [bp+int_10_ah],al
	mov	al,byte [video_mode]
	mov	byte [bp+int_10_al],al
	mov	al,byte [video_page]
	mov	byte [bp+int_10_bh],al
	ret

vid_check_mode:
	push	ax
	mov	al,byte [video_mode]
	cmp	al,07h
	jz	.exit
	cmp	al,04h
	cmc
	jnc	.exit
	sbb	al,al
	stc

.exit:
	pop	ax
	ret

vid_crtc_writew:
	mov	al,ch
	call	vid_crtc_writeb
	inc	ah
	mov	al,cl

vid_crtc_writeb:
	push	dx
	mov	dx,word [video_port]
	xchg	al,ah
	out	dx,al
	xchg	al,ah
	inc	dl
	out	dx,al
	pop	dx
	ret

vid_pixel_address:
	xor	si,si
	shr	dl,1
	jnb	.even
	mov	si,2000h

.even:
	mov	al,50h
	mul	dl

	add	si,ax
	mov	dx,cx
	mov	cx,0302h
	cmp	byte [video_mode],6
	pushf
	jnz	.1
	mov	cx,0703h

.1:
	and	ch,dl
	shr	dx,cl
	add	si,dx
	xchg	cl,ch
	popf
	ret

vid_current_offset:
	mov	bh,0
	shl	bx,1
	mov	ax,word [bx+video_cur_pos]

vid_position_to_offset:
	push	bx
	mov	bl,al
	mov	al,ah
	mul	byte [video_columns]
	mov	bh,0
	add	ax,bx
	shl	ax,1
	pop	bx
	ret

vid_gfx_pos_to_offset:
	push	bx
	mov	bl,al
	mov	al,ah
	mul	byte [video_columns]
	shl	ax,1
	shl	ax,1
	mov	bh,0
	add	ax,bx
	pop	bx
	ret

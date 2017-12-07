; This routine is the _main routine of the program.  It will set
; up a stack in chip memory and then call the "real" main.

	SECTION text,code
	DS.l 0

	XDEF	_main		; Make this the main routine
	XREF	_c_main   	; The "real" main function
	XREF	_LVOStackSwap	; exec stack swap routine
	XREF	_LVOAllocVec	; exec memory allocation routine
	XREF	_LVOFreeVec	; exec memory deallocation routine

STACKSPACE	EQU	2048	; amount of stack to allocate (max seen: 1378)

stk_Lower	EQU	$0	; Taken from <exec/tasks.i>
stk_Upper	EQU	$4	; Taken from <exec/tasks.i>
stk_Pointer	EQU	$8	; Taken from <exec/tasks.i>
MEMF_CHIP	EQU	$2	; Taken from <exec/memory.i>


_main:
        move.l  $4(sp),a3               ; Preserve argc
        move.l  $8(sp),a5               ; Preserve argv

	movea.l	4,a6			; Grab ExecBase
	move.l	#STACKSPACE+12,d0	; Also Allocate StackSwap Structure
	moveq.l #MEMF_CHIP,d1           ; Need chip memory
	jsr	_LVOAllocVec(a6)	; Allocate space for a StackSwap struct
	tst.l	d0
	beq	goexit			; Exit - no memory available

	move.l	d0,a2			; Save the allocated address

	add.l	#12,d0			; Bump past the StackSwap structure
	move.l	d0,stk_Lower(a2)	; Set Lower bound
	add.l	#STACKSPACE,d0		; Size+Lower bound
	move.l	d0,stk_Upper(a2)	; Set Upper bound
	sub.l	#4,d0			; Current stack pointer position
	move.l	d0,stk_Pointer(a2)	; Set current stack pointer

	movea.l	a2,a0			; Get StackSwap struct pointer
	jsr	_LVOStackSwap(a6)	; Change to new stack

        move.l  a5,-(sp)                ; argv
        move.l  a3,-(sp)                ; argc
	jsr	_c_main		        ; Execute program
        move.l  d0,a3
	adda.l	#8,sp	

	movea.l	a2,a0			; Get StackSwap struct pointer
	jsr	_LVOStackSwap(a6)	; Change back to original stack
	move.l	a2,a1			; Get allocated memory address
	jsr	_LVOFreeVec(a6)		; Free StackSwap and stack

        move.l  a3,d0
goexit:
	rts				; exit program, condition normal

        SECTION text,code
        DS.l 0

        XDEF    _burst_copy
        XDEF    _burst_copyline
        XDEF    _burst_read_moveml
        XDEF    _burst_read_readl
        XDEF    _burst_test_read
        XDEF    _mmu_get_tc_030
        XDEF    _mmu_set_tc_030
        XDEF    _mmu_get_tc_040
        XDEF    _mmu_set_tc_040
        XDEF    _cpu_dcache_flush
        XDEF    _get_sr
        XDEF    _irq_disable
        XDEF    _irq_enable
        XREF    _SysBase

RAMSEY_CONTROL  EQU     $00de0003
SysBase         EQU     $00000004
AttnFlags       EQU     $0128

AFB_68020       EQU     1
AFB_68030       EQU     2
AFB_68040       EQU     3
AFB_68060       EQU     7

CACRF_ClearD    EQU     $0800   ; Clear data cache (CD = Bit 11)

;
; void burst_test_read(APTR *buffer, APTR *test_addr, uint8_t ramsey_control);
;     Call this function from Supervisor state
;     $4(sp) is where to place burst read data
;     $8(sp) is address being read (under test)
;     $c(sp) is temporary Ramsey control register
_burst_test_read:
        move.l $4(sp),a0
        move.l $8(sp),a1
        move.l $c(sp),d0
        movem.l a2/d2-d6,-(sp)
        move.b RAMSEY_CONTROL,d1    ; Save Ramsey state
        move.b d0,RAMSEY_CONTROL    ; Enable Ramsey burst
wait1:
        move.b RAMSEY_CONTROL,d2
        cmp.b  d0,d2
        bne    wait1

        movem.l (a1),d3-d6          ; Burst read data
;       move.l $0(a1),d3
;       move.l $4(a1),d4
;       move.l $8(a1),d5
;       move.l $c(a1),d6
        move.b d1,RAMSEY_CONTROL    ; Restore Ramsey state
wait2:
        move.b RAMSEY_CONTROL,d2
        cmp.b  d1,d2
        bne    wait2
; Disable cache
; Write data
        move.l d6,$c(a0)            ; Do not burst the write
        move.l d5,$8(a0)
        move.l d4,$4(a0)
        move.l d3,$0(a0)

;; Enable interrupts
;        and.w  #$f8ff,sr
        movem.l (sp)+,a2/d2-d6
        rts

;
; void burst_copyline(APTR *dst, APTR *src);
;     $4(sp) is dst
;     $8(sp) is src
_burst_copyline:
        move.l $4(sp),a0
        move.l $8(sp),a1
        movem.l d2-d3,-(sp)
        move.l $0(a1),d0
        move.l $4(a1),d1
        move.l $8(a1),d2
        move.l $c(a1),d3
        move.l d3,$c(a0)            ; Do not burst the write
        move.l d2,$8(a0)
        move.l d1,$4(a0)
        move.l d0,$0(a0)
        movem.l (sp)+,d2-d3
        rts                         ; condition normal

; void burst_copy(APTR *dst, APTR *src, uint len);
;     $4(sp) is dst
;     $8(sp) is src
;     $c(sp) is len
_burst_copy:
        move.l $4(sp),a0
        move.l $8(sp),a1
        move.l $c(sp),d0
        movem.l a2-a5/d4-d7,-(sp)
        lsr.l #5,d0                 ; 32 bytes per iteration
        bra bcopy_check
bcopy_loop:
        movem.l (a1)+,a2-a5/d4-d7
        movem.l a2-a5/d4-d7,(a0)
        add #32,a0
bcopy_check:
        dbf   d0,bcopy_loop         ; DICE Das equivalent of dbra
        movem.l (sp)+,a2-a5/d4-d7
        move.l a1,d0
        rts

; void burst_read_moveml(APTR *dst, uint len);
;     $4(sp) is dst
;     $8(sp) is len
_burst_read_moveml:
        move.l $4(sp),a0            ; a0 = base address
        move.l $8(sp),d0            ; d0 = transfer length (bytes)
        movem.l a2-a5/d4-d7,-(sp)
        lsr.l #8,d0                 ; 256 bytes per iteration
        bra breadml_check
breadml_loop:
        movem.l (a0)+,a2-a5/d4-d7
        movem.l (a0)+,a2-a5/d4-d7
        movem.l (a0)+,a2-a5/d4-d7
        movem.l (a0)+,a2-a5/d4-d7
        movem.l (a0)+,a2-a5/d4-d7
        movem.l (a0)+,a2-a5/d4-d7
        movem.l (a0)+,a2-a5/d4-d7
        movem.l (a0)+,a2-a5/d4-d7
breadml_check:
;       sub.l #1,d0
;       cmp.l #-1,d0
;       bne   breadml_loop
        dbf   d0,breadml_loop         ; DICE Das equivalent of dbra
        movem.l (sp)+,a2-a5/d4-d7
        rts

; void burst_read_readml(APTR *dst, uint len);
;     $4(sp) is dst
;     $8(sp) is len
_burst_read_readl:
        move.l $4(sp),a0            ; a0 = base address
        move.l $8(sp),d0            ; d0 = transfer length (bytes)
        movem.l a2-a5/d4-d7,-(sp)
        lsr.l #6,d0                 ; 256 bytes per iteration
        bra breadrl_check
breadrl_loop:
        move.l (a0)+,a2
        move.l (a0)+,a3
        move.l (a0)+,a4
        move.l (a0)+,a5
        move.l (a0)+,d4
        move.l (a0)+,d5
        move.l (a0)+,d6
        move.l (a0)+,d7
        move.l (a0)+,a2
        move.l (a0)+,a3
        move.l (a0)+,a4
        move.l (a0)+,a5
        move.l (a0)+,d4
        move.l (a0)+,d5
        move.l (a0)+,d6
        move.l (a0)+,d7
breadrl_check:
        dbf   d0,breadrl_loop         ; DICE Das equivalent of dbra
        movem.l (sp)+,a2-a5/d4-d7
        rts


; uint32_t mmu_get_tc_030(void);
;     This function only works on the 68030.
;     68040 and 68060 have different MMU instructions.
_mmu_get_tc_030:
;       move.l  d0,-(sp)
        subq.l  #4,sp
        dc.l    $f0174200       ; pmove.l tc,(sp)
        move.l  (sp)+,d0
        rts

; void mmu_set_tc_030(uint32_t tc);
;     This function only works on the 68030.
_mmu_set_tc_030:
        adda.l  #4,sp
        dc.l    $f0174000       ; pmove.l (sp),tc
        suba.l  #4,sp
        rts

; uint32_t mmu_get_tc_040(void);
;     This function only works on 68040 and 68060.
_mmu_get_tc_040:
        dc.l    $4e7a0003       ; movec.l tc,d0
        rts

; void mmu_set_tc_040(uint32_t tc);
;     This function only works on 68040 and 68060.
_mmu_set_tc_040:
        move.l  $4(sp),d0
        dc.l    $4e7b0003       ; movec.l d0,tc
        rts

;
; void cpu_dcache_flush(void);
;
_cpu_dcache_flush:
        move.l  SysBase,a0
        move.l  AttnFlags(a0),d0
        btst    #AFB_68040,d0
        bne.s   _cpu_dcache_flush_040
_cpu_dcache_flush_030:
        dc.w    $4E7A,$0002     ; movec cacr,d0
        or.l    #CACRF_ClearD,d0
        dc.w    $4E7B,$0002     ; movec d0,cacr
        rts
_cpu_dcache_flush_040:
_cpu_dcache_flush_060:
        dc.w    $f478           ; cpusha (data cache)
        rts

_irq_disable:
        move.w  sr,d0
        or.w    #$0700,sr
        rts

_irq_enable:
        move.w  sr,d0
        and.w   #$f8ff,sr
        rts

_get_sr:
        move.w  sr,d0
        rts

; debug junk
;       move.l a2,$07770000
;       move.l a0,$0777000c
;       move.l d0,$07770008
;       move.l #$00010000,d0
;       divu.l d1,#12

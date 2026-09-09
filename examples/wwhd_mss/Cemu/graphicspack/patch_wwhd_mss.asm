[WWHD_MSS]
moduleMatches = 0x475bd29f, 0xb7e748de

0x0200E6F0 = _cCt_Counter_rest:

.origin = codecave

_rpl_state:
.byte 0
.align 4

_rpl_module:
.int 0

_rpl_fn:
.int 0

_rpl_name:
.string "wwhd_mss"
.align 4

_rpl_export:
.string "rpl_cemu_entry"
.align 4

_rpl_missing:
.string "[wwhd_mss] OSDynLoad_Acquire(wwhd_mss) failed -- is wwhd_mss.rpl in the title code folder?\n"
.align 4

_rpl_noexport:
.string "[wwhd_mss] wwhd_mss.rpl loaded but has no rpl_cemu_entry\n"
.align 4

_rpl_ok:
.string "[wwhd_mss] rpl_cemu_entry resolved\n"
.align 4

_rpl_resolve:
    stwu  r1, -0x30(r1)
    mflr  r0
    stw   r0, 0x24(r1)

    lis   r3, _rpl_state@ha
    lbz   r4, _rpl_state@l(r3)
    cmpwi r4, 0
    bne   _rpl_resolve_done

    li    r4, 1
    stb   r4, _rpl_state@l(r3)

    lis   r3, _rpl_name@ha
    addi  r3, r3, _rpl_name@l
    lis   r4, _rpl_module@ha
    addi  r4, r4, _rpl_module@l
    bl    import.coreinit.OSDynLoad_Acquire

    lis   r3, _rpl_module@ha
    lwz   r3, _rpl_module@l(r3)
    cmpwi r3, 0
    bne   _rpl_have_module

    lis   r3, _rpl_missing@ha
    addi  r3, r3, _rpl_missing@l
    bl    import.coreinit.OSReport
    b     _rpl_resolve_done

_rpl_have_module:
    li    r4, 0
    lis   r5, _rpl_export@ha
    addi  r5, r5, _rpl_export@l
    lis   r6, _rpl_fn@ha
    addi  r6, r6, _rpl_fn@l
    bl    import.coreinit.OSDynLoad_FindExport

    lis   r3, _rpl_fn@ha
    lwz   r3, _rpl_fn@l(r3)
    cmpwi r3, 0
    bne   _rpl_report_ok

    lis   r3, _rpl_noexport@ha
    addi  r3, r3, _rpl_noexport@l
    bl    import.coreinit.OSReport
    b     _rpl_resolve_done

_rpl_report_ok:
    lis   r3, _rpl_ok@ha
    addi  r3, r3, _rpl_ok@l
    bl    import.coreinit.OSReport

_rpl_resolve_done:
    lis   r9, _rpl_fn@ha
    lwz   r9, _rpl_fn@l(r9)
    lwz   r0, 0x24(r1)
    mtlr  r0
    addi  r1, r1, 0x30
    blr

counter_hook:
    stwu  r1, -0x40(r1)
    mflr  r0
    stw   r0, 0x34(r1)
    stw   r3, 0x10(r1)

    bl    _rpl_resolve
    cmpwi r9, 0
    beq   counter_done

    mtctr r9
    li    r3, 0
    li    r4, 0
    li    r5, 0
    li    r6, 0
    bctrl

counter_done:
    lwz   r3, 0x10(r1)
    lwz   r0, 0x34(r1)
    mtlr  r0
    addi  r1, r1, 0x40
    lis   r10, 0x1020
    b     _cCt_Counter_rest

vpad_read_hook:
    stwu  r1, -0x30(r1)
    mflr  r0
    stw   r0, 0x24(r1)
    stw   r4, 0x10(r1)

    bl    import.vpad.VPADRead
    stw   r3, 0x0c(r1)

    cmpwi r3, 0
    ble   vpad_done

    lis   r9, _rpl_fn@ha
    lwz   r9, _rpl_fn@l(r9)
    cmpwi r9, 0
    beq   vpad_done

    mtctr r9
    li    r3, 2
    lwz   r4, 0x10(r1)
    lwz   r5, 0x0c(r1)
    li    r6, 0
    bctrl

vpad_done:
    lwz   r3, 0x0c(r1)
    lwz   r0, 0x24(r1)
    mtlr  r0
    addi  r1, r1, 0x30
    blr

kpad_read_hook:
    stwu  r1, -0x30(r1)
    mflr  r0
    stw   r0, 0x24(r1)
    stw   r3, 0x14(r1)
    stw   r4, 0x10(r1)

    bl    import.padscore.KPADReadEx
    stw   r3, 0x0c(r1)

    cmpwi r3, 0
    ble   kpad_done

    lis   r9, _rpl_fn@ha
    lwz   r9, _rpl_fn@l(r9)
    cmpwi r9, 0
    beq   kpad_done

    mtctr r9
    li    r3, 4
    lwz   r4, 0x10(r1)
    lwz   r5, 0x0c(r1)
    lwz   r6, 0x14(r1)
    bctrl

kpad_done:
    lwz   r3, 0x0c(r1)
    lwz   r0, 0x24(r1)
    mtlr  r0
    addi  r1, r1, 0x30
    blr


gx2_copy_hook:
    stwu  r1, -0x30(r1)
    mflr  r0
    stw   r0, 0x24(r1)
    stw   r3, 0x10(r1)
    stw   r4, 0x14(r1)

    bl    _rpl_resolve
    cmpwi r9, 0
    beq   gx2_copy_fallback

    mtctr r9
    li    r3, 6
    lwz   r4, 0x10(r1)
    lwz   r5, 0x14(r1)
    li    r6, 0
    bctrl
    cmpwi r3, 0
    bne   gx2_copy_done

gx2_copy_fallback:
    lwz   r3, 0x10(r1)
    lwz   r4, 0x14(r1)
    bl    import.gx2.GX2CopyColorBufferToScanBuffer

gx2_copy_done:
    lwz   r0, 0x24(r1)
    mtlr  r0
    addi  r1, r1, 0x30
    blr


gx2_context_hook:
    stwu  r1, -0x30(r1)
    mflr  r0
    stw   r0, 0x24(r1)
    stw   r3, 0x10(r1)

    bl    _rpl_resolve
    cmpwi r9, 0
    beq   gx2_context_fallback

    mtctr r9
    li    r3, 8
    lwz   r4, 0x10(r1)
    li    r5, 0
    li    r6, 0
    bctrl
    cmpwi r3, 0
    bne   gx2_context_done

gx2_context_fallback:
    lwz   r3, 0x10(r1)
    bl    import.gx2.GX2SetContextState

gx2_context_done:
    lwz   r0, 0x24(r1)
    mtlr  r0
    addi  r1, r1, 0x30
    blr

0x0200E6EC = b   counter_hook
0x02035274 = bla gx2_context_hook

[WWHD_MSS_USA]
moduleMatches = 0x475bd29f

0x0273E420 = bla vpad_read_hook
0x0273D93C = bla kpad_read_hook
0x0274C59C = bla gx2_context_hook
0x0274C5D0 = bla gx2_context_hook
0x0274C630 = bla gx2_context_hook
0x0274C664 = bla gx2_context_hook
0x02750E88 = bla gx2_context_hook
0x02750EC0 = bla gx2_context_hook
0x02750EE8 = bla gx2_context_hook
0x02750F00 = bla gx2_context_hook
0x02750F38 = bla gx2_context_hook
0x02750F68 = bla gx2_context_hook
0x02750F98 = bla gx2_context_hook
0x027510A4 = bla gx2_copy_hook
0x0276B150 = ba  gx2_context_hook
0x0276B158 = ba  gx2_context_hook
0x027B9938 = bla gx2_copy_hook
0x027B9958 = bla gx2_copy_hook

[WWHD_MSS_EUR]
moduleMatches = 0xb7e748de

0x0273ECDC = bla vpad_read_hook
0x0273E1F8 = bla kpad_read_hook
0x0274CE58 = bla gx2_context_hook
0x0274CE8C = bla gx2_context_hook
0x0274CEEC = bla gx2_context_hook
0x0274CF20 = bla gx2_context_hook
0x02751744 = bla gx2_context_hook
0x0275177C = bla gx2_context_hook
0x027517A4 = bla gx2_context_hook
0x027517BC = bla gx2_context_hook
0x027517F4 = bla gx2_context_hook
0x02751824 = bla gx2_context_hook
0x02751854 = bla gx2_context_hook
0x02751960 = bla gx2_copy_hook
0x0276BA10 = ba  gx2_context_hook
0x0276BA18 = ba  gx2_context_hook
0x027BA1F8 = bla gx2_copy_hook
0x027BA218 = bla gx2_copy_hook

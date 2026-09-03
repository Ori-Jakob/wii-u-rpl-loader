# WUT's crt0_rpl.s never restores LR which breaks entry returns.
# We replace wuts crt0_rpl.s with this one during compile-time
# to fix it.

    .section .crt0, "ax", @progbits
    .global __rpl_loader_start
    .type   __rpl_loader_start, @function

__rpl_loader_start:
    stwu    1, -0x20(1)
    mflr    0
    stw     0, 0x24(1)        # LR, in the caller's frame as EABI wants it
    stw     3, 0x08(1)        # module handle
    stw     4, 0x0C(1)        # reason
    cmpwi   4, 2              # OS_DYNLOAD_UNLOADED
    beq     unload

load:
    bl      __init_wut
    bl      __init_wut_malloc
    bl      __init
    lwz     3, 0x08(1)
    lwz     4, 0x0C(1)
    bl      rpl_entry
    b       done

unload:
    lwz     3, 0x08(1)
    lwz     4, 0x0C(1)
    bl      rpl_entry
    stw     3, 0x10(1)        # keep rpl_entry's result
    bl      __fini
    bl      __fini_wut_malloc
    bl      __fini_wut
    lwz     3, 0x10(1)

done:
    lwz     0, 0x24(1)
    mtlr    0
    addi    1, 1, 0x20
    blr

    .size __rpl_loader_start, . - __rpl_loader_start

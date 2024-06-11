#------------------------------------------------------------------------------
#
# Debug related direct register handling support functions.
#
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
#------------------------------------------------------------------------------

.text
.p2align 2

GCC_ASM_EXPORT(DebugReadMdscrEl1)
GCC_ASM_EXPORT(DebugWriteMdscrEl1)
GCC_ASM_EXPORT(DebugReadOslsrEl1)
GCC_ASM_EXPORT(DebugWriteOslarEl1)
GCC_ASM_EXPORT(DebugReadDaif)
GCC_ASM_EXPORT(DebugWriteDaif)
GCC_ASM_EXPORT(DebugGetTCR)
GCC_ASM_EXPORT(DebugGetTTBR0BaseAddress)
GCC_ASM_EXPORT(DebugReadDbgWvr0El1)
GCC_ASM_EXPORT(DebugWriteDbgWvr0El1)
GCC_ASM_EXPORT(DebugReadDbgWcr0El1)
GCC_ASM_EXPORT(DebugWriteDbgWcr0El1)
GCC_ASM_EXPORT(DebugReadDbgWvr1El1)
GCC_ASM_EXPORT(DebugWriteDbgWvr1El1)
GCC_ASM_EXPORT(DebugReadDbgWcr1El1)
GCC_ASM_EXPORT(DebugWriteDbgWcr1El1)
GCC_ASM_EXPORT(DebugReadDbgWvr2El1)
GCC_ASM_EXPORT(DebugWriteDbgWvr2El1)
GCC_ASM_EXPORT(DebugReadDbgWcr2El1)
GCC_ASM_EXPORT(DebugWriteDbgWcr2El1)
GCC_ASM_EXPORT(DebugReadDbgWvr3El1)
GCC_ASM_EXPORT(DebugWriteDbgWvr3El1)
GCC_ASM_EXPORT(DebugReadDbgWcr3El1)
GCC_ASM_EXPORT(DebugWriteDbgWcr3El1)

ASM_PFX(DebugReadMdscrEl1):
    mrs x0, mdscr_el1
    ret

ASM_PFX(DebugWriteMdscrEl1):
    msr mdscr_el1, x0
    ret

ASM_PFX(DebugReadOslsrEl1):
    mrs x0, oslsr_el1
    ret

ASM_PFX(DebugWriteOslarEl1):
    msr oslar_el1, x0
    ret

ASM_PFX(DebugReadDaif):
    mrs x0, daif
    ret

ASM_PFX(DebugWriteDaif):
    msr daif, x0
    ret

ASM_PFX(DebugGetTCR):
    mrs x1, CurrentEL
    cmp x1, #0x8
    blt 1f
    b 2f
1:
    mrs x0, tcr_el1
    b 3f
2:
    mrs x0, tcr_el2
3:
    isb
    ret

ASM_PFX(DebugGetTTBR0BaseAddress):
    mrs x1, CurrentEL
    cmp x1, #0x8
    blt 1f
    b 2f
1:
    mrs x0, ttbr0_el1
    b 3f
2:
    mrs x0, ttbr0_el2
3:
    ldr x1, =0xFFFFFFFFFFFF
    and x0, x0, x1
    isb
    ret

ASM_PFX(DebugReadDbgWvr0El1):
    mrs x0, dbgwvr0_el1
    ret

ASM_PFX(DebugWriteDbgWvr0El1):
    msr dbgwvr0_el1, x0
    ret

ASM_PFX(DebugReadDbgWcr0El1):
    mrs x0, dbgwcr0_el1
    ret

ASM_PFX(DebugWriteDbgWcr0El1):
    msr dbgwcr0_el1, x0
    ret

ASM_PFX(DebugReadDbgWvr1El1):
    mrs x0, dbgwvr1_el1
    ret

ASM_PFX(DebugWriteDbgWvr1El1):
    msr dbgwvr1_el1, x0
    ret

ASM_PFX(DebugReadDbgWcr1El1):
    mrs x0, dbgwcr1_el1
    ret

ASM_PFX(DebugWriteDbgWcr1El1):
    msr dbgwcr1_el1, x0
    ret

ASM_PFX(DebugReadDbgWvr2El1):
    mrs x0, dbgwvr2_el1
    ret

ASM_PFX(DebugWriteDbgWvr2El1):
    msr dbgwvr2_el1, x0
    ret

ASM_PFX(DebugReadDbgWcr2El1):
    mrs x0, dbgwcr2_el1
    ret

ASM_PFX(DebugWriteDbgWcr2El1):
    msr dbgwcr2_el1, x0
    ret

ASM_PFX(DebugReadDbgWvr3El1):
    mrs x0, dbgwvr3_el1
    ret

ASM_PFX(DebugWriteDbgWvr3El1):
    msr dbgwvr3_el1, x0
    ret

ASM_PFX(DebugReadDbgWcr3El1):
    mrs x0, dbgwcr3_el1
    ret

ASM_PFX(DebugWriteDbgWcr3El1):
    msr dbgwcr3_el1, x0
    ret

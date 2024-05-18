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

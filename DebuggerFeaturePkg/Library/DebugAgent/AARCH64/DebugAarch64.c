/** @file
  Implements ARM64 specific debugger routines.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/WatchdogTimerLib.h>
#include <Library/TimerLib.h>
#include <Protocol/DebugSupport.h>

#include "DebugAgent.h"
#include "GdbStub.h"
#include "AArch64Mmu.h"

//
// Debug register definitions.
//

#define ARM64_MDSCR_MDE   0x00008000
#define ARM64_MDSCR_KDE   0x00002000
#define ARM64_MDSCR_TDCC  0x00001000
#define ARM64_MDSCR_SS    0x00000001

//
// Assembly routines.
//

UINT64
DebugReadMdscrEl1 (
  VOID
  );

VOID
DebugWriteMdscrEl1 (
  IN UINT64  Value
  );

UINT64
DebugGetTCR (
  VOID
  );

UINT64
DebugGetTTBR0BaseAddress (
  VOID
  );

//
// Structures used by the arch-agnostic code.
//

UINT8  ArchBreakpointInstruction[]   = { 0x00, 0x00, 0x3E, 0xD4 };
UINTN  ArchBreakpointInstructionSize = sizeof (ArchBreakpointInstruction);

UINT32  ArchExceptionTypes[] = {
  EXCEPT_AARCH64_SYNCHRONOUS_EXCEPTIONS,
  MAX_UINT32 // End of list
};

/**
  This routine handles synchronous exceptions.

  @param  InterruptType     Supplies the type of the exception.
  @param  SystemContext     Supplies the system context at the time of the exception.

  N.B. For more information about ARM64 exception handling, download the ARMV8
       architecture reference manual from https://developer.arm.com/.

**/
VOID
EFIAPI
DebugExceptionHandler (
  EFI_EXCEPTION_TYPE  InterruptType,
  EFI_SYSTEM_CONTEXT  SystemContext
  )
{
  EFI_SYSTEM_CONTEXT_AARCH64  *Context;
  EXCEPTION_INFO              ExceptionInfo;
  UINT64                      ExceptionType;
  BOOLEAN                     WatchdogState;
  UINT64                      Value;

  //
  // Suspend the watchdog while handling debug events
  // Even simple debug events, like symbol loading,
  // can wait in the debugger if there was a pending break-in.
  //
  WatchdogState = WatchdogSuspend ();

  Context = SystemContext.SystemContextAArch64;
  ZeroMem (&ExceptionInfo, sizeof (EXCEPTION_INFO));

  //
  // The upper 6 bits of the ECR provides the exception type. The
  // EFI exception are not sufficient since they are limited
  // for AARCH64 and do not contain things like breakpoint or
  // step exceptions.
  //

  ExceptionType = (0x3F & (Context->ESR >> 26));

  switch (ExceptionType) {
    case 0x00: // Illegal opcode
      ExceptionInfo.ExceptionType    = ExceptionInvalidOp;
      ExceptionInfo.ExceptionAddress = Context->ELR;
      break;

    case 0x20: // Lower EL instruction abort
    case 0x21: // Current EL instruction abort
    case 0x24: // Lower EL data abort
    case 0x25: // Current EL data abort
      ExceptionInfo.ExceptionType    = ExceptionGenericFault;
      ExceptionInfo.ExceptionAddress = Context->ELR;
      break;

    case 0x22: // PC alignment
    case 0x26: // Stack alignment

      ExceptionInfo.ExceptionType    = ExceptionAlignment;
      ExceptionInfo.ExceptionAddress = Context->ELR;
      break;

    case 0x30: // Lower EL hardware breakpoint
    case 0x31: // Current EL hardware breakpoint
    case 0x34: // Lower EL Watchpoint break
    case 0x35: // Current EL Watchpoint break
    case 0x3c: // BRK Instruction

      ExceptionInfo.ExceptionType    = ExceptionBreakpoint;
      ExceptionInfo.ExceptionAddress = Context->ELR;
      break;

    case 0x32: // Lower EL single step
    case 0x33: // Current EL single step

      // Clear the step flag is present.
      Value  = DebugReadMdscrEl1 ();
      Value &= ~ARM64_MDSCR_SS;
      DebugWriteMdscrEl1 (Value);

      ExceptionInfo.ExceptionType    = ExceptionDebugStep;
      ExceptionInfo.ExceptionAddress = Context->ELR;
      break;

    default:

      //
      // Miscellaneous unhandled situations
      //
      CpuDeadLoop ();
  }

  ExceptionInfo.ArchExceptionCode = ExceptionType;

  ReportEntryToDebugger (&ExceptionInfo, SystemContext);

  //
  // If this is a breakpoint, step past it.
  //

  if ((ExceptionType == 0x3c) &&
      (CompareMem ((UINT8 *)Context->ELR, &ArchBreakpointInstruction[0], ArchBreakpointInstructionSize) == 0))
  {
    Context->ELR += ArchBreakpointInstructionSize;
  }

  //
  // Resume the watchdog.
  //
  WatchdogResume (WatchdogState);

  return;
}

/**
  Adds or removes a single step from to the system context.

  @param[in,out]  SystemContext     The system context to add the single step to.

**/
VOID
AddSingleStep (
  IN OUT EFI_SYSTEM_CONTEXT  *SystemContext
  )
{
  UINT64  Value;

  // Clear the DEBUG bit if set. This could be set because the debug exception are
  // originally enabled from a outside of an exception. If this bit is set though
  // then the SS bit will not be respected.
  SystemContext->SystemContextAArch64->SPSR &= ~BIT9;

  // Set the Software Step bit in the SPSR.
  SystemContext->SystemContextAArch64->SPSR |= BIT21;

  // Set the Software Step bit in the MDSCR.
  Value  = DebugReadMdscrEl1 ();
  Value |= (ARM64_MDSCR_SS | ARM64_MDSCR_MDE | ARM64_MDSCR_KDE);
  DebugWriteMdscrEl1 (Value);
}

/**
  Gets the performance counter in milliseconds.

  @retval   Current performance count converted to milliseconds.
**/
UINT64
DebugGetTimeMs (
  VOID
  )
{
  UINT64  Freq;

  //
  // ARM64 has discoverable timer frequency, so the timer lib should be immediately,
  // available. Direct access to the CNT registers can be added if needed.
  //

  Freq = GetPerformanceCounterProperties (NULL, NULL);
  ASSERT (Freq >= 1000);
  Freq /= 1000;
  return GetPerformanceCounter () / Freq;
}

/**
  Enables ARM64 debug controls

  @param[in]  DebugConfig       The debug configuration data.

**/
VOID
DebugArchInit (
  IN DEBUGGER_CONTROL_HOB  *DebugConfig
  )
{
  UINT64  Value;

  Value  = DebugReadMdscrEl1 ();
  Value |= (ARM64_MDSCR_MDE | ARM64_MDSCR_KDE);
  DebugWriteMdscrEl1 (Value);
}

#define MIN_T0SZ        16
#define BITS_PER_LEVEL  9
#define MAX_VA_BITS     48

/**
  Recursively checks a page table for a virtual address entry.

  @param[in]  TranslationTable    Pointer to the next page table.
  @param[in]  TableLevel          The level of the next page table.
  @param[in]  LastBlockEntry      The last block in the page table.
  @param[in]  Address             The address to look for.
  @param[out] Attributes          Returns the attributes of the virtual address.

  @retval     TRUE        The address was found.
  @retval     FALSE       The address was not found.
**/
BOOLEAN
ParsePageTableLevel (
  IN  UINT64  *TranslationTable,
  IN  UINTN   TableLevel,
  IN  UINT64  *LastBlockEntry,
  IN  UINTN   Address,
  OUT UINTN   *Attributes
  )
{
  UINT64  *NextTranslationTable;
  UINT64  *BlockEntry;
  UINT64  BlockEntryType;
  UINT64  EntryType;

  if (TableLevel != 3) {
    BlockEntryType = TT_TYPE_BLOCK_ENTRY;
  } else {
    BlockEntryType = TT_TYPE_BLOCK_ENTRY_LEVEL3;
  }

  // Find the block entry linked to the Base Address
  BlockEntry = (UINT64 *)TT_GET_ENTRY_FOR_ADDRESS (TranslationTable, TableLevel, Address);
  EntryType  = *BlockEntry & TT_TYPE_MASK;

  if ((TableLevel < 3) && (EntryType == TT_TYPE_TABLE_ENTRY)) {
    NextTranslationTable = (UINT64 *)(*BlockEntry & TT_ADDRESS_MASK_DESCRIPTION_TABLE);

    // The entry is a page table, so we go to the next level
    return ParsePageTableLevel (
             NextTranslationTable,                      // Address of the next level page table
             TableLevel + 1,                            // Next Page Table level
             (UINTN *)TT_LAST_BLOCK_ADDRESS (NextTranslationTable, TT_ENTRY_COUNT),
             Address,
             Attributes
             );
  } else if (EntryType == BlockEntryType) {
    *Attributes = *BlockEntry & TT_ATTRIBUTES_MASK;
    return TRUE;
  }

  return FALSE;
}

/**
  Checks if a virtual address is valid.

  @param[in]  Address     The virtual address to check.
  @param[in]  Write       Indicates the page needs to be writable.

  @retval     TRUE        The address is valid.
  @retval     FALSE       The address is not valid.
**/
BOOLEAN
CheckPageAccess (
  IN  UINTN    Address,
  IN  BOOLEAN  Write
  )
{
  UINT64   *TranslationTable;
  UINTN    TableLevel;
  UINTN    EntryCount;
  UINTN    T0SZ;
  BOOLEAN  Result;
  UINTN    Attributes;

  // This is a workaround. Windbg will try to read some KSEG addresses by default
  // which will never exist in UEFI because of the identity mapping requirements.
  // This shouldn't be required, but either some platforms have over-zealous page
  // table mappings or the page table walking logic is insufficient.
  if (Address >= 0xfffff00000000000) {
    return FALSE;
  }

  Attributes       = 0;
  TranslationTable = (UINT64 *)DebugGetTTBR0BaseAddress ();
  T0SZ             = DebugGetTCR () & TCR_T0SZ_MASK;
  TableLevel       = (T0SZ - MIN_T0SZ) / BITS_PER_LEVEL;
  EntryCount       = TT_ENTRY_COUNT >> (T0SZ - MIN_T0SZ) % BITS_PER_LEVEL;

  Result = ParsePageTableLevel (
             TranslationTable,
             TableLevel,
             (UINTN *)TT_LAST_BLOCK_ADDRESS (TranslationTable, EntryCount),
             Address,
             &Attributes
             );

  if (!Result) {
    return FALSE;
  }

  // Ignore device memory. This can be blanket mapped.
  if ((Attributes & TT_ATTR_INDX_MASK) == TT_ATTR_INDX_DEVICE_MEMORY) {
    return FALSE;
  } else if (Write) {
    return ((Attributes & TT_AP_RW_RW) != 0) || ((Attributes & TT_AP_MASK) == 0);
  } else {
    return (Attributes & TT_AF) != 0;
  }
}

/**
  Checks if a given virtual address is readable.

  @param[in]  Address     The virtual address to check.

  @retval     TRUE        The address is readable.
  @retval     FALSE       The address is not readable.
**/
BOOLEAN
IsPageReadable (
  IN UINT64  Address
  )
{
  return CheckPageAccess (Address, FALSE);
}

/**
  Checks if a given virtual address is writable.

  @param[in]  Address     The virtual address to check.

  @retval     TRUE        The address is writable.
  @retval     FALSE       The address is not writable.
**/
BOOLEAN
IsPageWritable (
  IN UINT64  Address
  )
{
  return CheckPageAccess (Address, TRUE);
}

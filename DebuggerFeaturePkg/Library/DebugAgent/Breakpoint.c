/** @file
  Contains the implementation of the debug agent breakpoints.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "DebugAgent.h"

#include <Library/CacheMaintenanceLib.h>

//
// Structure used to track software breakpoints.
//

#define MAX_BREAKPOINT_SIZE  4
#define MAX_BREAKPOINTS      64

typedef struct _BREAKPOINT_INFO {
  BOOLEAN    Active;
  UINTN      Address;
  UINT8      OriginalValue[MAX_BREAKPOINT_SIZE];
} BREAKPOINT_INFO;

BREAKPOINT_INFO  mBreakpoints[MAX_BREAKPOINTS];

BREAKPOINT_REASON  DebuggerBreakpointReason = BreakpointReasonNone;

/**
  Adds a software breakpoint as the specific address.

  @param[in]  Address   The virtual address of the location of the breakpoint.

  @retval   TRUE   The breakpoint was successfully added.
  @retval   FALSE  The breakpoint was not added.
**/
BOOLEAN
AddSoftwareBreakpoint (
  UINTN  Address
  )
{
  UINTN            i;
  BREAKPOINT_INFO  *Entry;

  // Make sure we don't overflow OriginalValue.
  ASSERT (ArchBreakpointInstructionSize <= MAX_BREAKPOINT_SIZE);

  Entry = NULL;
  for (i = 0; i < MAX_BREAKPOINTS; i++) {
    if (mBreakpoints[i].Active && (mBreakpoints[i].Address == Address)) {
      // Duplicate, no need to do anything, return TRUE.
      return TRUE;
    }

    if (!mBreakpoints[i].Active && (Entry == NULL)) {
      Entry = &mBreakpoints[i];
    }
  }

  if (Entry == NULL) {
    ASSERT (FALSE);
    return FALSE;
  }

  Entry->Active  = TRUE;
  Entry->Address = Address;
  DbgReadMemory (Address, &(Entry->OriginalValue[0]), ArchBreakpointInstructionSize);
  DbgWriteMemory (Address, ArchBreakpointInstruction, ArchBreakpointInstructionSize);
  InvalidateInstructionCacheRange ((VOID *)Address, ArchBreakpointInstructionSize);
  return TRUE;
}

/**
  Removes a software breakpoint as the specific address.

  @param[in]  Address   The virtual address of the location of the breakpoint.

  @retval   TRUE   The breakpoint was successfully added.
  @retval   FALSE  The breakpoint was not added.
**/
BOOLEAN
RemoveSoftwareBreakpoint (
  UINTN  Address
  )
{
  UINTN  i;

  for (i = 0; i < MAX_BREAKPOINTS; i++) {
    if (mBreakpoints[i].Active && (mBreakpoints[i].Address == Address)) {
      DbgWriteMemory (Address, &(mBreakpoints[i].OriginalValue[0]), ArchBreakpointInstructionSize);
      mBreakpoints[i].Active = FALSE;
      InvalidateInstructionCacheRange ((VOID *)Address, ArchBreakpointInstructionSize);
      return TRUE;
    }
  }

  // Not found.
  return FALSE;
}

/**
  Immediately breaks into the debugger.

  @param[in]  Reason    The reason for the debug break.

**/
VOID
DebuggerBreak (
  BREAKPOINT_REASON  Reason
  )
{
  DebuggerBreakpointReason = Reason;
  CpuBreakpoint ();
  DebuggerBreakpointReason = BreakpointReasonNone;
}

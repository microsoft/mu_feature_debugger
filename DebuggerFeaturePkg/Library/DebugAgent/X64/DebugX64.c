/**@file Exception.c

  This file contains X64 debug functions.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Protocol/Cpu.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/SerialPortLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/WatchdogTimerLib.h>

#include <Register/Intel/Cpuid.h>

#include "DebugAgent.h"
#include "GdbStub.h"

#define TF_BIT  0x00000100

// Debug registers defines.
#define DR7_ENABLE_MASK  0xFF
#define DR7_WRITE_ONLY   0b01
#define DR7_READ_WRITE   0b11

typedef union _X64_DR7 {
  struct {
    UINTN    L0         : 1;  // Local breakpoint enable
    UINTN    G0         : 1;  // Global breakpoint enable
    UINTN    L1         : 1;  // Local breakpoint enable
    UINTN    G1         : 1;  // Global breakpoint enable
    UINTN    L2         : 1;  // Local breakpoint enable
    UINTN    G2         : 1;  // Global breakpoint enable
    UINTN    L3         : 1;  // Local breakpoint enable
    UINTN    G3         : 1;  // Global breakpoint enable
    UINTN    LE         : 1;  // Local exact breakpoint enable
    UINTN    GE         : 1;  // Global exact breakpoint enable
    UINTN    Reserved_1 : 3;  // Reserved
    UINTN    GD         : 1;  // Global detect enable
    UINTN    Reserved_2 : 2;  // Reserved
    UINTN    RW0        : 2;  // Read/Write field
    UINTN    LEN0       : 2;  // Length field
    UINTN    RW1        : 2;  // Read/Write field
    UINTN    LEN1       : 2;  // Length field
    UINTN    RW2        : 2;  // Read/Write field
    UINTN    LEN2       : 2;  // Length field
    UINTN    RW3        : 2;  // Read/Write field
    UINTN    LEN3       : 2;  // Length field
    UINTN    Reserved_3 : 32; // Length field
  } Bits;
  UINTN    UintN;
} X64_DR7;

extern EFI_CPU_ARCH_PROTOCOL  *gCpu;
STATIC UINT64                 mPerformanceCounterFreq;

//
// Structures used by the arch-agnostic code.
//

UINT8  ArchBreakpointInstruction[]   = { 0xCC };
UINTN  ArchBreakpointInstructionSize = sizeof (ArchBreakpointInstruction);

UINT32  ArchExceptionTypes[] = {
  EXCEPT_X64_DIVIDE_ERROR,
  EXCEPT_X64_DEBUG,
  EXCEPT_X64_BREAKPOINT,
  EXCEPT_X64_DOUBLE_FAULT,
  EXCEPT_X64_GP_FAULT,
  EXCEPT_X64_PAGE_FAULT,
  EXCEPT_X64_SEG_NOT_PRESENT,
  EXCEPT_X64_NMI,
  MAX_UINT32 // End of list
};

/**
  This routine handles synchronous exceptions.

  @param[in]  InterruptType     Supplies the type of the exception.
  @param[in]  SystemContext     Supplies the system context at the time of the exception.

  N.B. For more information about X64 exception handling, download Intel's
       software developer manuals at
       https://software.intel.com/en-us/articles/intel-sdm.

**/
VOID
EFIAPI
DebuggerExceptionHandler (
  EFI_EXCEPTION_TYPE  InterruptType,
  EFI_SYSTEM_CONTEXT  SystemContext
  )
{
  EFI_SYSTEM_CONTEXT_X64  *Context;
  EXCEPTION_INFO          ExceptionInfo;
  BOOLEAN                 WatchdogState;

  //
  // Suspend the watchdog while handling debug events
  // Even simple debug events, like symbol loading,
  // can wait in the debugger if there was a pending break-in.
  //
  WatchdogState = WatchdogSuspend ();

  Context = SystemContext.SystemContextX64;
  ZeroMem (&ExceptionInfo, sizeof (EXCEPTION_INFO));

  switch (InterruptType) {
    case EXCEPT_X64_DEBUG:
      Context->Rflags               &= (UINT64) ~TF_BIT;   // Clear any single step flag
      ExceptionInfo.ExceptionType    = ExceptionDebugStep;
      ExceptionInfo.ExceptionAddress = Context->Rip;
      break;

    case EXCEPT_X64_BREAKPOINT:
      ExceptionInfo.ExceptionType = ExceptionBreakpoint;
      //
      // Decrement this as 0xCC op code is a trap with RIP pointing after the instruction.
      //
      Context->Rip                  -= 1;
      ExceptionInfo.ExceptionAddress = Context->Rip;
      break;

    case EXCEPT_X64_PAGE_FAULT:
      ExceptionInfo.ExceptionType    = ExceptionAccessViolation;
      ExceptionInfo.ExceptionAddress = Context->Rip;
      break;

    case EXCEPT_X64_DOUBLE_FAULT:
    case EXCEPT_X64_SEG_NOT_PRESENT:
    case EXCEPT_X64_GP_FAULT:
    default:
      ExceptionInfo.ExceptionType    = ExceptionGenericFault;
      ExceptionInfo.ExceptionAddress = Context->Rip;
      break;
  }

  ExceptionInfo.ArchExceptionCode = InterruptType;

  // Call into the core debugger module.
  ReportEntryToDebugger (&ExceptionInfo, SystemContext);

  switch (InterruptType) {
    case EXCEPT_X64_BREAKPOINT:
      //
      // Increment past the 0xCC op code is a trap with RIP pointing after the instruction.
      //
      if (*((UINT8 *)Context->Rip) == 0xCC) {
        Context->Rip++;
      }

      break;

    default:
      break;
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
  SystemContext->SystemContextX64->Rflags |= TF_BIT;
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
  return AsmReadTsc () / mPerformanceCounterFreq;
}

/**
  Initializes x64 specific debug configurations.

  @param[in]  DebugConfig       The debug configuration data.

**/
VOID
DebugArchInit (
  IN DEBUGGER_CONTROL_HOB  *DebugConfig
  )
{
  // Disable hardware breakpoints.
  UINTN  Dr7;

  // Disable first.
  Dr7 = AsmReadDr7 ();
  AsmWriteDr7 (Dr7 & ~DR7_ENABLE_MASK);

  // Set stashed TSC frequency
  mPerformanceCounterFreq = DebugConfig->PerformanceCounterFreq;
}

/**
  Converts a byte count length to the x64 debug register representation. Unsupported
  sizes will default to 1 byte.

  @param[in]  Length    The byte count length.

  @retval   UINTN the x64 debug register representation.
**/
UINTN
LengthToDebugRegLen (
  IN UINTN  Length
  )
{
  switch (Length) {
    case 8:
      return 0b10;
    case 4:
      return 0b11;
    case 2:
      return 0b01;
    case 1:
    default:
      return 0b00;
  }
}

/**
  Adds a X64 hardware watch point.

  @param[in]  Address   The address of the data watch point.
  @param[in]  Length    The length of the data watch point.
  @param[in]  Read      Boolean indicated break on read.
  @param[in]  Write     Boolean indicated break on write.

  @retval  TRUE   The watch point was successfully set.
  @retval  FALSE  The watch point could not be set.
**/
BOOLEAN
AddWatchpoint (
  IN UINTN    Address,
  IN UINTN    Length,
  IN BOOLEAN  Read,
  IN BOOLEAN  Write
  )
{
  X64_DR7  Dr7;
  UINTN    Rw;
  UINTN    Len;

  // Only READ is not supported, so only check only write condition
  Rw        = Read ? DR7_READ_WRITE : DR7_WRITE_ONLY;
  Len       = LengthToDebugRegLen (Length);
  Dr7.UintN = AsmReadDr7 ();

  // Check for duplicate.
  if (Dr7.Bits.L0 && (Dr7.Bits.RW0 == Rw) && (Dr7.Bits.LEN0 == Len) && (AsmReadDr0 () == Address)) {
    return TRUE;
  } else if (Dr7.Bits.L1 && (Dr7.Bits.RW1 == Rw) && (Dr7.Bits.LEN1 == Len) && (AsmReadDr1 () == Address)) {
    return TRUE;
  } else if (Dr7.Bits.L2 && (Dr7.Bits.RW2 == Rw) && (Dr7.Bits.LEN2 == Len) && (AsmReadDr2 () == Address)) {
    return TRUE;
  } else if (Dr7.Bits.L3 && (Dr7.Bits.RW3 == Rw) && (Dr7.Bits.LEN3 == Len) && (AsmReadDr3 () == Address)) {
    return TRUE;
  }

  // find a free breakpoint.
  if (!Dr7.Bits.L0) {
    AsmWriteDr0 (Address);
    Dr7.Bits.L0   = 1;
    Dr7.Bits.RW0  = Rw;
    Dr7.Bits.LEN0 = Len;
  } else if (!Dr7.Bits.L1) {
    AsmWriteDr1 (Address);
    Dr7.Bits.L1   = 1;
    Dr7.Bits.RW1  = Rw;
    Dr7.Bits.LEN1 = Len;
  } else if (!Dr7.Bits.L2) {
    AsmWriteDr2 (Address);
    Dr7.Bits.L2   = 1;
    Dr7.Bits.RW2  = Rw;
    Dr7.Bits.LEN2 = Len;
  } else if (!Dr7.Bits.L3) {
    AsmWriteDr3 (Address);
    Dr7.Bits.L3   = 1;
    Dr7.Bits.RW3  = Rw;
    Dr7.Bits.LEN3 = Len;
  } else {
    return FALSE;
  }

  AsmWriteDr7 (Dr7.UintN);
  return TRUE;
}

/**
  Removes a X64 hardware watch point.

  @param[in]  Address   The address of the data watch point.
  @param[in]  Length    The length of the data watch point.
  @param[in]  Read      Boolean indicated break on read.
  @param[in]  Write     Boolean indicated break on write.

  @retval  TRUE   The watch point was successfully removed.
  @retval  FALSE  The watch point could not be removed or did not exist.
**/
BOOLEAN
RemoveWatchpoint (
  IN UINTN    Address,
  IN UINTN    Length,
  IN BOOLEAN  Read,
  IN BOOLEAN  Write
  )
{
  X64_DR7  Dr7;
  UINT8    Rw;
  UINTN    Len;

  // Only READ is not supported, so only check only write condition
  Rw        = Read ? DR7_READ_WRITE : DR7_WRITE_ONLY;
  Len       = LengthToDebugRegLen (Length);
  Dr7.UintN = AsmReadDr7 ();

  // Check for duplicate.
  if (Dr7.Bits.L0 && (Dr7.Bits.RW0 == Rw) && (Dr7.Bits.LEN0 == Len) && (AsmReadDr0 () == Address)) {
    Dr7.Bits.L0 = 0;
  } else if (Dr7.Bits.L1 && (Dr7.Bits.RW1 == Rw) && (Dr7.Bits.LEN1 == Len) && (AsmReadDr1 () == Address)) {
    Dr7.Bits.L1 = 0;
  } else if (Dr7.Bits.L2 && (Dr7.Bits.RW2 == Rw) && (Dr7.Bits.LEN2 == Len) && (AsmReadDr2 () == Address)) {
    Dr7.Bits.L2 = 0;
  } else if (Dr7.Bits.L3 && (Dr7.Bits.RW3 == Rw) && (Dr7.Bits.LEN3 == Len) && (AsmReadDr3 () == Address)) {
    Dr7.Bits.L3 = 0;
  } else {
    return FALSE;
  }

  AsmWriteDr7 (Dr7.UintN);
  return TRUE;
}

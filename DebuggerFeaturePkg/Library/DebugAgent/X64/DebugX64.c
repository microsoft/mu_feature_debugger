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
      ExceptionInfo.ExceptionType    = ExceptionGenericFault;
      ExceptionInfo.ExceptionAddress = Context->Rip;
      break;

    default:

      //
      // Unhandled situations
      //

      CpuDeadLoop ();
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
  mPerformanceCounterFreq = DebugConfig->PerformanceCounterFreq;
}

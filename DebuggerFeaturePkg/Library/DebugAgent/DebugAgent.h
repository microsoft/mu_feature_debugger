/**@file
  DebugAgent.h

  This file contains definitions and prototypes for internal functions used within
  DebugAgentLib for DxeCore.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __DEBUG_AGENT_H__
#define __DEBUG_AGENT_H__

#include <Uefi.h>

#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include <Pi/PiHob.h>
#include <DebuggerControlHob.h>

//
// For debugging the debugger.
//

// #define DBG_DEBUG  1

#ifdef DBG_DEBUG

#define DBG_LOG_SIZE  (0x1000)
extern CHAR8  DbgLogBuffer[DBG_LOG_SIZE];
extern UINTN  DbgLogOffset;
#define DBG_LOG(LogString, ...) \
  DbgLogOffset += AsciiSPrint (&DbgLogBuffer[DbgLogOffset], \
                               (DBG_LOG_SIZE - DbgLogOffset), \
                               LogString, \
                               ##__VA_ARGS__);

#else
#define DBG_LOG(LogString, ...)
#endif

//
// Architecture agnostic structure for exceptions.
//

typedef enum _EXCEPTION_TYPE {
  ExceptionDebugStep = 0,
  ExceptionBreakpoint,
  ExceptionGenericFault,
  ExceptionInvalidOp,
  ExceptionAlignment,
  ExceptionAccessViolation
} EXCEPTION_TYPE;

typedef struct _EXCEPTION_INFO {
  EXCEPTION_TYPE    ExceptionType;
  UINT64            ExceptionAddress;
  UINT64            ArchExceptionCode;

  // TODO: Add union with information like PF address.
} EXCEPTION_INFO;

//
// Architecture specific definitions used by general debugger code.
//

extern UINT8   ArchBreakpointInstruction[];
extern UINTN   ArchBreakpointInstructionSize;
extern UINT32  ArchExceptionTypes[];

//
// Global used to track debugger invoked breakpoint.
//

typedef enum _BREAKPOINT_REASON {
  BreakpointReasonNone = 0,
  BreakpointReasonInitial,
  BreakpointReasonModuleLoad,
  BreakpointReasonDebuggerBreak,
} BREAKPOINT_REASON;

extern BREAKPOINT_REASON  DebuggerBreakpointReason;

//
// Global used for debugger information.
//

extern CONST CHAR8  *gDebuggerInfo;

VOID
EFIAPI
DebuggerExceptionHandler (
  EFI_EXCEPTION_TYPE  InterruptType,
  EFI_SYSTEM_CONTEXT  SystemContext
  );

EFI_STATUS
DebugAgentExceptionInitialize (
  );

VOID
DebugAgentExceptionDestroy (
  );

VOID
DebugReboot (
  );

BOOLEAN
DbgSetBreakOnModuleLoad (
  IN CHAR8  *Module
  );

//
// Address check routines
//

BOOLEAN
IsPageReadable (
  IN UINT64  Address
  );

BOOLEAN
IsPageWritable (
  IN UINT64  Address
  );

//
// Memory Access Routines
//

BOOLEAN
DbgReadMemory (
  UINTN  Address,
  VOID   *Data,
  UINTN  Length
  );

BOOLEAN
DbgWriteMemory (
  UINTN  Address,
  VOID   *Data,
  UINTN  Length
  );

//
// IO process module
//

VOID
ReportEntryToDebugger (
  IN EXCEPTION_INFO          *ExceptionInfo,
  IN OUT EFI_SYSTEM_CONTEXT  SystemContext
  );

VOID
DebuggerPollInput (
  VOID
  );

VOID
DebuggerInitialBreakpoint (
  UINT64  Timeout
  );

//
// Breakpoints.
//

BOOLEAN
AddSoftwareBreakpoint (
  UINTN  Address
  );

BOOLEAN
RemoveSoftwareBreakpoint (
  UINTN  Address
  );

VOID
DebuggerBreak (
  BREAKPOINT_REASON  Reason
  );

//
// Architecture specific
//

VOID
AddSingleStep (
  IN OUT EFI_SYSTEM_CONTEXT  *SystemContext
  );

UINT64
DebugGetTimeMs (
  VOID
  );

VOID
DebugArchInit (
  IN DEBUGGER_CONTROL_HOB  *DebugConfig
  );

BOOLEAN
AddWatchpoint (
  IN UINTN    Address,
  IN UINTN    Length,
  IN BOOLEAN  Read,
  IN BOOLEAN  Write
  );

BOOLEAN
RemoveWatchpoint (
  IN UINTN    Address,
  IN UINTN    Length,
  IN BOOLEAN  Read,
  IN BOOLEAN  Write
  );

#endif

/** @file DebugAgentPeiLib.c

  Implementation of the DebugAgentLib for PEI phase (post-memory only).

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/CpuExceptionHandlerLib.h>
#include <Library/DebugLib.h>
#include <Library/DebugAgentLib.h>
#include <Library/DebugTransportLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/TimerLib.h>
#include <Library/TransportLogControlLib.h>
#include <Library/WatchdogTimerLib.h>

#include <DebuggerControlHob.h>

#include "DebugAgent.h"

CONST CHAR8  *gDebuggerInfo = "PEI UEFI Debugger";

//
// Default PEI debug configuration. No timeout, initial breakpoint enabled.
//

DEBUGGER_CONTROL_HOB  DefaultPeiDebugConfig = {
  .Control.AsUint32         = 0x1,      // InitialBreakpoint only
  .PerformanceCounterFreq   = 0x300000, // Reasonable guess, timing may be inaccurate.
  .InitialBreakpointTimeout = 0         // No timeout
};

//
// Debug log buffers
//

#ifdef DBG_DEBUG
CHAR8  DbgLogBuffer[DBG_LOG_SIZE];
UINTN  DbgLogOffset = 0;
#endif

/**
  This routine removes the exception handling support.

**/
VOID
DebugAgentExceptionDestroy (
  )
{
  UINT8  Index;

  for (Index = 0; mArchExceptionTypes[Index] != MAX_UINT32; Index += 1) {
    RegisterCpuInterruptHandler (mArchExceptionTypes[Index], NULL);
  }
}

/**
  This routine initializes the debugger exception handling support.

  @retval EFI_SUCCESS       On success.
  @retval EFI_STATUS        On failure.

**/
EFI_STATUS
DebugAgentExceptionInitialize (
  )
{
  UINT8       Index;
  EFI_STATUS  Status;

  Status = InitializeCpuExceptionHandlers (NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: InitializeCpuExceptionHandlers failed - %r\n", __func__, Status));
    return Status;
  }

  for (Index = 0; mArchExceptionTypes[Index] != MAX_UINT32; Index += 1) {
    Status = RegisterCpuInterruptHandler (mArchExceptionTypes[Index], DebuggerExceptionHandler);
    if (EFI_ERROR (Status)) {
      ASSERT_EFI_ERROR (Status);
      DebugAgentExceptionDestroy ();
      return Status;
    }
  }

  return Status;
}

/**
  Reboots the system.

**/
VOID
DebugReboot (
  VOID
  )
{
  // NOT IMPLEMENTED for PEI.
  return;
}

/**
  Read system memory. In PEI post-mem, all memory is directly accessible.

  @param[in]      Address   The virtual address of the memory access.
  @param[out]     Data      The buffer to read memory into.
  @param[in]      Length    The length of the memory range.

  @retval         TRUE      Memory access was complete successfully.
  @retval         FALSE     Memory access failed, either completely or partially.
**/
BOOLEAN
DbgReadMemory (
  IN  UINTN  Address,
  OUT VOID   *Data,
  IN  UINTN  Length
  )
{
  if (!IsPageReadable (Address)) {
    return FALSE;
  }

  CopyMem (Data, (VOID *)Address, Length);
  return TRUE;
}

/**
  Write to system memory. In PEI post-mem, all memory is directly accessible.

  @param[in]      Address   The virtual address of the memory access.
  @param[in]      Data      The buffer of data to write.
  @param[in]      Length    The length of the memory range.

  @retval         TRUE      Memory access was complete successfully.
  @retval         FALSE     Memory access failed, either completely or partially.
**/
BOOLEAN
DbgWriteMemory (
  IN UINTN  Address,
  IN VOID   *Data,
  IN UINTN  Length
  )
{
  if (!IsPageWritable (Address)) {
    return FALSE;
  }

  CopyMem ((VOID *)Address, Data, Length);
  return TRUE;
}

/**
  Setup the debugger to break when a particular module is loaded.

  @param[in]  Module   The name of the module.

  @retval  TRUE   The break on module was set.
  @retval  FALSE  The break on module was not set.

**/
BOOLEAN
DbgSetBreakOnModuleLoad (
  IN CHAR8  *Module
  )
{
  // NOT SUPPORTED in PEI.
  return FALSE;
}

/**
  Initialize debug agent.

  When called with DEBUG_AGENT_INIT_PEI, performs the full PEI debug agent
  initialization: transport, architecture-specific setup, exception handlers,
  and initial breakpoint.

  @param[in] InitFlag     Init flag. DEBUG_AGENT_INIT_PEI to initialize.
  @param[in] Context      Context needed according to InitFlag; it was optional.
  @param[in] Function     Continue function called by debug agent library; it was
                          optional.

**/
VOID
EFIAPI
InitializeDebugAgent (
  IN UINT32                InitFlag,
  IN VOID                  *Context  OPTIONAL,
  IN DEBUG_AGENT_CONTINUE  Function  OPTIONAL
  )
{
  EFI_STATUS  Status;

  if (!PcdGetBool (PcdForceEnablePeiDebugger)) {
    DEBUG ((DEBUG_INFO, "%a: PEI debugger not enabled.\n", __func__));
    return;
  }

  Status = DebugTransportInitialize ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Transport init failed - %r\n", __func__, Status));
    return;
  }

  DebugArchInit (&DefaultPeiDebugConfig);

  Status = DebugAgentExceptionInitialize ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Exception init failed - %r\n", __func__, Status));
    return;
  }

  DEBUG ((DEBUG_INFO, "%a: PEI debug agent initialized.\n", __func__));

  //
  // Always perform an initial breakpoint with no timeout for PEI.
  //
  DebuggerInitialBreakpoint (0);
}

/**
  Enable/Disable the interrupt of debug timer and return the interrupt state
  prior to the operation.

  If EnableStatus is TRUE, enable the interrupt of debug timer.
  If EnableStatus is FALSE, disable the interrupt of debug timer.

  @param[in] EnableStatus    Enable/Disable.

  @return FALSE always.

**/
BOOLEAN
EFIAPI
SaveAndSetDebugTimerInterrupt (
  IN BOOLEAN  EnableStatus
  )
{
  return FALSE;
}

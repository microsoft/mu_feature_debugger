/**@file DebugAgent.c

  Implementation of the DebugAgent for MM.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugAgentLib.h>
#include <Library/CpuExceptionHandlerLib.h>
#include <Library/DebugTransportLib.h>
#include <Library/HobLib.h>

#include "DebugAgent.h"

// Reaches into DxeCore for earlier access.
CONST CHAR8  *gDebuggerInfo = "MM UEFI Debugger";

DEBUGGER_CONTROL_HOB  DefaultDebugConfig = {
  { 0x5 },
  0x300000, // Reasonable guess, timing may be inaccurate.
  0
};

//
// Debug log buffers
//

#ifdef DBG_DEBUG
CHAR8  DbgLogBuffer[DBG_LOG_SIZE];
UINTN  DbgLogOffset = 0;
#endif

//
// MM externs. Because of the flat nature of MM, this must be statically linked
// and is not yet part of a library.
//

EFI_STATUS
EFIAPI
SmmGetMemoryAttributes (
  IN  EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN  UINT64                Length,
  IN  UINT64                *Attributes
  );

EFI_STATUS
SmmSetMemoryAttributes (
  IN  EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN  UINT64                Length,
  IN  UINT64                Attributes
  );

EFI_STATUS
SmmClearMemoryAttributes (
  IN  EFI_PHYSICAL_ADDRESS  BaseAddress,
  IN  UINT64                Length,
  IN  UINT64                Attributes
  );

//
// Global variables used to track state and pointers.
//
STATIC BOOLEAN  mDebuggerInitialized;

/**
  This routine removes the KdDxe exception handling support.

**/
VOID
DebugAgentExceptionDestroy (
  )
{
  UINT8  i;

  for (i = 0; ArchExceptionTypes[i] != MAX_UINT32; i += 1) {
    RegisterCpuInterruptHandler (ArchExceptionTypes[i], NULL);
  }
}

/**
  This routine initializes the debugger exception handling support on X64.

  @retval EFI_SUCCESS       On success.
  @retval EFI_STATUS        On failure.

**/
EFI_STATUS
DebugAgentExceptionInitialize (
  )
{
  UINT8       i;
  EFI_STATUS  Status;

  // First uninstall any handler that needs to be replaced.
  DebugAgentExceptionDestroy ();

  for (i = 0; ArchExceptionTypes[i] != MAX_UINT32; i += 1) {
    Status = RegisterCpuInterruptHandler (ArchExceptionTypes[i], DebuggerExceptionHandler);
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
  // NOT IMPLEMENTED YET.
  return;
}

/**
  Access memory on the system after validating the memory is valid and has
  the required attributes.

  @param[in]      Address   The virtual address of the memory access.
  @param[in,out]  Data      The buffer to read memory from or into.
  @param[in]      Length    The length of the memory range.
  @param[in]      Write     FALSE for read operations, TRUE for write.

  @retval         TRUE      Memory access was complete successfully.
  @retval         FALSE     Memory access failed, either completely or partially.
**/
BOOLEAN
AccessMemory (
  UINTN    Address,
  UINT8    *Data,
  UINTN    Length,
  BOOLEAN  Write
  )
{
  UINTN       LengthInPage;
  UINT64      Attributes;
  BOOLEAN     AttributesChanged;
  EFI_STATUS  Status;

  //
  // Go page by page, making sure the attributes are correct for every page.
  //

  while (Length > 0) {
    LengthInPage      = MIN (Length, EFI_PAGE_SIZE - (EFI_PAGE_MASK & Address));
    AttributesChanged = FALSE;
    Attributes        = 0;

    Status = SmmGetMemoryAttributes (Address & ~EFI_PAGE_MASK, EFI_PAGE_SIZE, &Attributes);
    if (EFI_ERROR (Status)) {
      return FALSE;
    }

    if (Write && (Attributes & EFI_MEMORY_RO)) {
      Status = SmmClearMemoryAttributes (
                 Address & ~EFI_PAGE_MASK,
                 EFI_PAGE_SIZE,
                 EFI_MEMORY_RO | EFI_MEMORY_RP
                 );

      if (EFI_ERROR (Status)) {
        return FALSE;
      }

      AttributesChanged = TRUE;
    } else if (Attributes & EFI_MEMORY_RP) {
      Status = SmmClearMemoryAttributes (
                 Address & ~EFI_PAGE_MASK,
                 EFI_PAGE_SIZE,
                 EFI_MEMORY_RP
                 );

      if (EFI_ERROR (Status)) {
        return FALSE;
      }

      AttributesChanged = TRUE;
    }

    if (Write) {
      CopyMem ((VOID *)Address, Data, LengthInPage);
    } else {
      CopyMem (Data, (VOID *)Address, LengthInPage);
    }

    // Restore attributes
    if (AttributesChanged) {
      Status = SmmSetMemoryAttributes (
                 Address & ~EFI_PAGE_MASK,
                 EFI_PAGE_SIZE,
                 Attributes
                 );
    }

    // Move the address and data forward.
    Address += LengthInPage;
    Data    += LengthInPage;
    Length  -= LengthInPage;
  }

  return TRUE;
}

/**
  Read system memory.

  @param[in]      Address   The virtual address of the memory access.
  @param[in,out]  Data      The buffer to read memory into.
  @param[in]      Length    The length of the memory range.

  @retval         TRUE      Memory access was complete successfully.
  @retval         FALSE     Memory access failed, either completely or partially.
**/
BOOLEAN
DbgReadMemory (
  UINTN  Address,
  VOID   *Data,
  UINTN  Length
  )
{
  return AccessMemory (Address, Data, Length, FALSE);
}

/**
  Write to system memory.

  @param[in]      Address   The virtual address of the memory access.
  @param[in,out]  Data      The buffer of data to write.
  @param[in]      Length    The length of the memory range.

  @retval         TRUE      Memory access was complete successfully.
  @retval         FALSE     Memory access failed, either completely or partially.
**/
BOOLEAN
DbgWriteMemory (
  UINTN  Address,
  VOID   *Data,
  UINTN  Length
  )
{
  return AccessMemory (Address, Data, Length, TRUE);
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
  // NOT SUPPORTED.
  return FALSE;
}

/**
  Initialize debug agent.

  This function is used to set up the debugger interface.

  If the parameter Function is not NULL, Debug Agent Library instance will invoke it by
  passing in the Context to be its parameter.

  If Function() is NULL, Debug Agent Library instance will return after setup debug
  environment.

  @param[in] InitFlag     InitFlag is used to decide the initialize process.
  @param[in] Context      Context needed according to InitFlag; it was optional.
  @param[in] Function     Continue function called by debug agent library; it was
                          optional.

**/
VOID
EFIAPI
InitializeDebugAgent (
  IN UINT32                InitFlag,
  IN VOID                  *Context   OPTIONAL,
  IN DEBUG_AGENT_CONTINUE  Function  OPTIONAL
  )
{
  EFI_HOB_GUID_TYPE     *GuidHob;
  DEBUGGER_CONTROL_HOB  *DebugHob;
  EFI_STATUS            Status;

  DEBUG ((DEBUG_INFO, "%a: Entry.\n", __FUNCTION__));

  if (InitFlag == DEBUG_AGENT_INIT_SMM) {
    if (PcdGetBool (PcdForceEnableDebugger)) {
      DebugHob = &DefaultDebugConfig;
    } else {
      // Check if the HOB has been provided, otherwise bail.
      GuidHob = GetNextGuidHob (&gDebuggerControlHobGuid, Context);
      if (GuidHob == NULL) {
        return;
      }

      DebugHob = (DEBUGGER_CONTROL_HOB *)GET_GUID_HOB_DATA (GuidHob);
      if (!DebugHob->Control.Flags.MmDebugEnabled) {
        return;
      }
    }

    Status = DebugTransportInitialize ();
    if (EFI_ERROR (Status)) {
      return;
    }

    DebugArchInit (DebugHob);

    Status = DebugAgentExceptionInitialize ();
    if (EFI_ERROR (Status)) {
      return;
    }

    mDebuggerInitialized = TRUE;

    //
    // If requested, call the initial breakpoint.
    //

    if (DebugHob->Control.Flags.InitialBreakpoint) {
      DebuggerInitialBreakpoint (DebugHob->InitialBreakpointTimeout);
    }
  } else if (InitFlag == DEBUG_AGENT_INIT_ENTER_SMI) {
    if (mDebuggerInitialized) {
      DebuggerPollInput ();
    }
  } else if (InitFlag == 0) {
    // Special case for DebugApp to indicate DebugApp is terminating.
    if (mDebuggerInitialized) {
      DebugAgentExceptionDestroy ();
    }
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Unsupported call to MmCore DebugAgent (0x%x)\n", __FUNCTION__, InitFlag));
  }

  DEBUG ((DEBUG_INFO, "%a: Exit.\n", __FUNCTION__));
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

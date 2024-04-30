/**@file DebugAgent.c

  Implementation of the DebugAgent for DXE.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Protocol/Cpu.h>
#include <Protocol/Timer.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/MemoryAttribute.h>

#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugAgentLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PeCoffGetEntryPointLib.h>
#include <Library/CpuExceptionHandlerLib.h>
#include <Library/DebugTransportLib.h>
#include <Library/HobLib.h>
#include <Library/ResetSystemLib.h>

#include "DebugAgent.h"

// Reaches into DxeCore for earlier access.
extern EFI_CPU_ARCH_PROTOCOL    *gCpu;
extern EFI_TIMER_ARCH_PROTOCOL  *gTimer;
extern EFI_BOOT_SERVICES        mBootServices;
extern EFI_RUNTIME_SERVICES     *gDxeCoreRT;

STATIC EFI_MEMORY_ATTRIBUTE_PROTOCOL  *mMemoryAttributeProtocol = NULL;

CONST CHAR8  *gDebuggerInfo = "DXE UEFI Debugger";

//
// Debug log buffers
//

#ifdef DBG_DEBUG
CHAR8  DbgLogBuffer[DBG_LOG_SIZE];
UINTN  DbgLogOffset = 0;
#endif

DEBUGGER_CONTROL_HOB  DefaultDebugConfig = {
  .Control.AsUint32 = 0x3,
  0x300000, // Reasonable guess, timing may be inaccurate.
  0
};

//
// Global variables used to track state and pointers.
//
STATIC VOID       *mCpuRegistration              = NULL;
STATIC VOID       *mTimerRegistration            = NULL;
STATIC VOID       *mLoadedImageRegistration      = NULL;
STATIC VOID       *mMemoryAttributesRegistration = NULL;
STATIC EFI_EVENT  mTimerEvent;
STATIC EFI_EVENT  mCpuArchEvent;
STATIC EFI_EVENT  mLoadedImageEvent;
STATIC EFI_EVENT  mExitBootServicesEvent;
STATIC BOOLEAN    mDebuggerInitialized;
STATIC CHAR8      mDbgBreakOnModuleLoadString[64] = { 0 };

/**
  This routine handles timer events.

  @param  Event            Not used.
  @param  Context          Not used.
**/
VOID
EFIAPI
DebugAgentTimerRoutine (
  EFI_EVENT  Event,
  VOID       *Context
  )
{
  DebuggerPollInput ();
}

/**
  This routine initializes timer events to check every second for a possible
  request to break in.

  N.B. Any failures in this routine are intentionally ignored.  Control-C
       functionality will not work without timers, but exception handling will
       still work.

**/
VOID
DebugAgentInitializeTimer (
  )
{
  EFI_STATUS  Status;

  Status = gBS->CreateEvent (
                  EVT_TIMER | EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  DebugAgentTimerRoutine,
                  NULL,
                  &mTimerEvent
                  );

  if (EFI_ERROR (Status) == FALSE) {
    Status = gBS->SetTimer (
                    mTimerEvent,
                    TimerPeriodic,
                    EFI_TIMER_PERIOD_SECONDS (1)
                    );

    DEBUG ((DEBUG_INFO, "%a: Setting Timer Event. Code=%r\n", __FUNCTION__, Status));
  }

  return;
}

/**
  This routine terminates the timer event.

**/
VOID
DebugAgentTimerDestroy (
  )
{
  if (mTimerEvent != NULL) {
    gBS->CloseEvent (mTimerEvent);
  }

  return;
}

/**
  This routine handles the EXIT_BOOT_SERVICES notification, and terminates
  the debugger

  @param  Event            Not used.
  @param  Context          Not used.

**/
VOID
EFIAPI
OnExitBootServices (
  EFI_EVENT  Event,
  VOID       *Context
  )
{
  DebugAgentTimerDestroy ();
  DebugAgentExceptionDestroy ();
  return;
}

/**
    Cpu Arch Protocol notification.

    @param    Event           Not Used.
    @param    Context         Not Used.

    @retval   none

    Initialize Debugger when Cpu Arch is available.

 **/
VOID
EFIAPI
OnCpuArchProtocolNotification (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;

  Status = gBS->LocateProtocol (&gEfiCpuArchProtocolGuid, NULL, (VOID **)&gCpu);
  if (EFI_ERROR (Status)) {
    return;
  }

  //
  // Initialize Exception Handling.
  //

  Status = DebugAgentExceptionInitialize ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: DebugAgentExceptionInitialize failed, Status = (%r).\n", __FUNCTION__, Status));
    ASSERT_EFI_ERROR (Status);
    OnExitBootServices (NULL, NULL);
    return;
  }
}

/**
    Timer Arch Protocol notification.

    @param    Event           Not Used.
    @param    Context         Not Used.

    @retval   none

    Initialize Debugger Polling.

 **/
VOID
EFIAPI
OnTimerArchProtocolNotification (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;

  Status = gBS->LocateProtocol (&gEfiTimerArchProtocolGuid, NULL, (VOID **)&gTimer);
  if (EFI_ERROR (Status)) {
    return;
  }

  DebugAgentInitializeTimer ();
}

/**
    Memory Attribute Protocol notification.

    @param    Event           Not Used.
    @param    Context         Not Used.

    @retval   none

    Initialize Debugger Polling.

 **/
VOID
EFIAPI
OnMemoryAttributeProtocolNotification (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS  Status;

  Status = gBS->LocateProtocol (
                  &gEfiMemoryAttributeProtocolGuid,
                  NULL,
                  (VOID **)&mMemoryAttributeProtocol
                  );

  if (EFI_ERROR (Status)) {
    return;
  }
}

/**
    Loaded Image Protocol notification.

    @param    Event           Not Used.
    @param    Context         Not Used.

    @retval   none

    Initialize Debugger Polling.

 **/
VOID
EFIAPI
OnLoadedImageNotification (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  UINTN                      BufferSize;
  EFI_HANDLE                 Handle;
  EFI_LOADED_IMAGE_PROTOCOL  *LoadedImage;
  EFI_STATUS                 Status;
  CHAR8                      *PdbPointer;
  CHAR8                      Name[64];
  CHAR8                      *NameEnd;
  UINTN                      i;

  BufferSize = sizeof (EFI_HANDLE);

  // If there is no break requested, then quick escape.
  if (AsciiStrLen (&mDbgBreakOnModuleLoadString[0]) == 0) {
    return;
  }

  while (TRUE) {
    Status = gBS->LocateHandle (
                    ByRegisterNotify,
                    NULL,
                    mLoadedImageRegistration,
                    &BufferSize,
                    &Handle
                    );

    if (EFI_ERROR (Status)) {
      break;
    }

    Status = gBS->HandleProtocol (
                    Handle,
                    &gEfiLoadedImageProtocolGuid,
                    (VOID **)&LoadedImage
                    );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: HandleProtocol failed, Status = %r\n", __FUNCTION__, Status));
      break;
    }

    PdbPointer = PeCoffLoaderGetPdbPointer (LoadedImage->ImageBase);
    if (PdbPointer == NULL) {
      continue;
    }

    // Strip of the directories.
    for (i = AsciiStrLen (PdbPointer); i > 0; i--) {
      if ((PdbPointer[i - 1] == '\\') || (PdbPointer[i - 1] == '/')) {
        PdbPointer = &PdbPointer[i];
        break;
      }
    }

    ASSERT (AsciiStrLen (PdbPointer) < sizeof (Name));
    CopyMem (&Name[0], PdbPointer, AsciiStrLen (PdbPointer) + 1);
    NameEnd = ScanMem8 (Name, AsciiStrLen (Name), '.');
    if (NameEnd != NULL) {
      *NameEnd = 0;
    }

    if (AsciiStrLen (Name) == AsciiStrLen (mDbgBreakOnModuleLoadString)) {
      if (AsciiStriCmp (mDbgBreakOnModuleLoadString, Name) == 0) {
        DebuggerBreak (BreakpointReasonModuleLoad);
        break;
      }
    }
  }

  return;
}

/**
  This routine removes the KdDxe exception handling support.

**/
VOID
DebugAgentExceptionDestroy (
  )
{
  UINT8  i;

  if (gCpu != NULL) {
    for (i = 0; ArchExceptionTypes[i] != MAX_UINT32; i += 1) {
      if (gCpu != NULL) {
        gCpu->RegisterInterruptHandler (
                gCpu,
                ArchExceptionTypes[i],
                NULL
                );
      } else {
        RegisterCpuInterruptHandler (ArchExceptionTypes[i], NULL);
      }
    }
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
    if (gCpu != NULL) {
      Status = gCpu->RegisterInterruptHandler (
                       gCpu,
                       ArchExceptionTypes[i],
                       DebuggerExceptionHandler
                       );
    } else {
      Status = RegisterCpuInterruptHandler (ArchExceptionTypes[i], DebuggerExceptionHandler);
    }

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
  ResetCold ();
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
  EFI_STATUS  Status;
  BOOLEAN     AttributesChanged;

  //
  // Go page by page, making sure the attributes are correct for every page.
  //

  while (Length > 0) {
    LengthInPage      = MIN (Length, EFI_PAGE_SIZE - (EFI_PAGE_MASK & Address));
    AttributesChanged = FALSE;
    Attributes        = 0;

    // Set attributes if needed.
    if (mMemoryAttributeProtocol != NULL) {
      Status = mMemoryAttributeProtocol->GetMemoryAttributes (
                                           mMemoryAttributeProtocol,
                                           Address & ~EFI_PAGE_MASK,
                                           EFI_PAGE_SIZE,
                                           &Attributes
                                           );
      if (EFI_ERROR (Status)) {
        return FALSE;
      }

      if (Write && (Attributes & EFI_MEMORY_RO)) {
        Status = mMemoryAttributeProtocol->ClearMemoryAttributes (
                                             mMemoryAttributeProtocol,
                                             Address & ~EFI_PAGE_MASK,
                                             EFI_PAGE_SIZE,
                                             EFI_MEMORY_RO | EFI_MEMORY_RP
                                             );
        if (EFI_ERROR (Status)) {
          return FALSE;
        }

        AttributesChanged = TRUE;
      } else if (Attributes & EFI_MEMORY_RP) {
        Status = mMemoryAttributeProtocol->ClearMemoryAttributes (
                                             mMemoryAttributeProtocol,
                                             Address & ~EFI_PAGE_MASK,
                                             EFI_PAGE_SIZE,
                                             EFI_MEMORY_RP
                                             );
        if (EFI_ERROR (Status)) {
          return FALSE;
        }

        AttributesChanged = TRUE;
      }
    } else if (Write) {
      if (!IsPageWritable (Address)) {
        return FALSE;
      }
    } else {
      if (!IsPageReadable (Address)) {
        return FALSE;
      }
    }

    if (Write) {
      CopyMem ((VOID *)Address, Data, LengthInPage);
    } else {
      CopyMem (Data, (VOID *)Address, LengthInPage);
    }

    // Restore attributes
    if (AttributesChanged && (mMemoryAttributeProtocol != NULL)) {
      Status = mMemoryAttributeProtocol->SetMemoryAttributes (
                                           mMemoryAttributeProtocol,
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
  UINTN  Len;

  Len = AsciiStrLen (Module);
  if (Len >= sizeof (mDbgBreakOnModuleLoadString)) {
    return FALSE;
  }

  CopyMem (&mDbgBreakOnModuleLoadString[0], Module, Len + 1);
  return TRUE;
}

/**
  Setup callbacks for all events required for the DXE phase debugger.

**/
EFI_STATUS
EFIAPI
DxeDebugSetupCallbacks (
  VOID
  )
{
  EFI_STATUS  Status;

  if (gCpu == NULL) {
    DEBUG ((DEBUG_INFO, "%a: Reset Notification protocol not installed. Registering for notification\n", __FUNCTION__));
    mCpuArchEvent = EfiCreateProtocolNotifyEvent (
                      &gEfiCpuArchProtocolGuid,
                      TPL_CALLBACK,
                      OnCpuArchProtocolNotification,
                      NULL,
                      &mCpuRegistration
                      );

    if (mCpuArchEvent == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: failed to create Cpu Arch Protocol Notify callback\n", __FUNCTION__));
    }
  }

  if (gTimer == NULL) {
    DEBUG ((DEBUG_INFO, "%a: Timer Arch protocol not installed. Registering for notification\n", __FUNCTION__));
    mTimerEvent = EfiCreateProtocolNotifyEvent (
                    &gEfiTimerArchProtocolGuid,
                    TPL_CALLBACK,
                    OnTimerArchProtocolNotification,
                    NULL,
                    &mTimerRegistration
                    );

    if (mTimerEvent == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: failed to create Timer Arch Protocol Notify callback\n", __FUNCTION__));
    }
  }

  mLoadedImageEvent = EfiCreateProtocolNotifyEvent (
                        &gEfiLoadedImageProtocolGuid,
                        TPL_CALLBACK,
                        OnLoadedImageNotification,
                        NULL,
                        &mLoadedImageRegistration
                        );

  if (mLoadedImageEvent == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: failed to create Loaded Image Protocol Notify callback\n", __FUNCTION__));
  }

  mLoadedImageEvent = EfiCreateProtocolNotifyEvent (
                        &gEfiMemoryAttributeProtocolGuid,
                        TPL_CALLBACK,
                        OnMemoryAttributeProtocolNotification,
                        NULL,
                        &mMemoryAttributesRegistration
                        );

  if (mLoadedImageEvent == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: failed to create Loaded Image Protocol Notify callback\n", __FUNCTION__));
  }

  //
  // Register for EXIT_BOOT_SERVICES notification.
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnExitBootServices,
                  NULL,
                  &gEfiEventExitBootServicesGuid,
                  &mExitBootServicesEvent
                  );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: failed to create Exit Boot Services callback\n", __FUNCTION__));
  }

  return EFI_SUCCESS;
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

  //
  // Initialize the debugger phase, take special care to not allow operations other
  // then DEBUG_AGENT_INIT_DXE_CORE if mDebuggerInitialized is not set.
  //

  if (InitFlag == DEBUG_AGENT_INIT_DXE_CORE) {
    if (PcdGetBool (PcdForceEnableDebugger)) {
      DebugHob = &DefaultDebugConfig;
    } else {
      // Check if the HOB has been provided, otherwise bail.
      GuidHob = GetNextGuidHob (&gDebuggerControlHobGuid, Context);
      if (GuidHob == NULL) {
        return;
      }

      DebugHob = (DEBUGGER_CONTROL_HOB *)GET_GUID_HOB_DATA (GuidHob);
      if (!DebugHob->Control.Flags.DxeDebugEnabled) {
        return;
      }
    }

    if (gBS == NULL) {
      gBS = &mBootServices;
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
  } else if (InitFlag == 0) {
    // Special case for DebugApp to indicate DebugApp is terminating.
    if (mDebuggerInitialized) {
      OnExitBootServices (NULL, NULL);
    }
  } else if (InitFlag == DEBUG_AGENT_INIT_DXE_CORE_LATE) {
    if (mDebuggerInitialized) {
      DxeDebugSetupCallbacks ();
    }
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Unsupported call to DxeCore DebugAgent (0x%x)\n", __FUNCTION__, InitFlag));
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

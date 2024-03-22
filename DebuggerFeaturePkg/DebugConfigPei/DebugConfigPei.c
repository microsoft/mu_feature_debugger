/** @file
  Implements a simple PEI module to configure the debugger.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <PiPei.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DeviceStateLib.h>
#include <DebuggerControlHob.h>

VOID
EFIAPI
ArchDebugConfig (
  DEBUGGER_CONTROL_HOB  *ConfigHob
  );

/**
  Checks device state and sets the debug policy HOB.

  @param[in]    FileHandle      UNUSED.
  @param[in]    PeiServices     UNUSED.

  @retval EFI_SUCCESS           The debug configuration was set successfully.
  @retval EFI_OUT_OF_RESOURCES  There is no additional space in the PPI database.
**/
EFI_STATUS
EFIAPI
DebugConfigPeiEntry (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  DEBUGGER_CONTROL_HOB  *DebugHob;

  // Skip if the device state flag is not set.
  if ((GetDeviceState () & DEVICE_STATE_SOURCE_DEBUG_ENABLED) == 0) {
    DEBUG ((DEBUG_INFO, "Debug agent will not be enabled.\n"));
    return EFI_SUCCESS;
  }

  DebugHob = BuildGuidHob (&gDebuggerControlHobGuid, sizeof (DEBUGGER_CONTROL_HOB));
  if (DebugHob == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ZeroMem (DebugHob, sizeof (DEBUGGER_CONTROL_HOB));

  // Move some PCDs into the HOB.
  DebugHob->Control.AsUint32         = PcdGet32 (PcdDebugConfigFlags);
  DebugHob->InitialBreakpointTimeout = PcdGet64 (PcdInitialBreakpointTimeoutMs);
  ArchDebugConfig (DebugHob);

  DEBUG ((
    DEBUG_INFO,
    "Debug agent enabled. Flags 0x%x InitalBreakpointTimeout %d\n",
    DebugHob->Control.AsUint32,
    DebugHob->InitialBreakpointTimeout
    ));

  return EFI_SUCCESS;
}

/** @file
  Thin PEIM wrapper that invokes the PEI debug agent library.

  This PEIM is dispatched after permanent memory is available (DEPEX on
  gEfiPeiMemoryDiscoveredPpiGuid). It simply calls InitializeDebugAgent
  to let the library perform all initialization.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Library/DebugAgentLib.h>

/**
  PEIM entry point. Invokes the debug agent library initialization.

  @param[in]  FileHandle      Handle of the file being loaded.
  @param[in]  PeiServices     Pointer to PEI Services table.

  @retval EFI_SUCCESS         Debug agent initialized successfully.

**/
EFI_STATUS
EFIAPI
DebugAgentPeiEntry (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  InitializeDebugAgent (DEBUG_AGENT_INIT_PEI, NULL, NULL);
  return EFI_SUCCESS;
}

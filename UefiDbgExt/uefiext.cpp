/*++

Copyright (c) Microsoft Corporation.

SPDX-License-Identifier: BSD-2-Clause-Patent

Module Name:

    uefiext.cpp

Abstract:

    This file contains core UEFI debug commands.

--*/

#include "uefiext.h"

UEFI_ENV  gUefiEnv = DXE;

HRESULT
NotifyOnTargetAccessible (
  PDEBUG_CONTROL  Control
  )
{
  //
  // Attempt to determine what environment the debugger is in.
  //

  return S_OK;
}

HRESULT CALLBACK
setenv (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  INIT_API ();

  if (_stricmp (args, "PEI") == 0) {
    gUefiEnv = PEI;
  } else if (_stricmp (args, "DXE") == 0) {
    gUefiEnv = DXE;
  } else if (_stricmp (args, "MM") == 0) {
    gUefiEnv = MM;
  } else {
    dprintf ("Unknown environment type! Supported types: PEI, DXE, MM");
  }

  EXIT_API ();
  return S_OK;
}

HRESULT CALLBACK
help (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  INIT_API ();

  UNREFERENCED_PARAMETER (args);

  dprintf (
    "Help for uefiext.dll\n"
    "\nBasic Commands:\n"
    "  help                - Shows this help\n"
    "  init                - Detects and initializes windbg for debugging UEFI.\n"
    "  setenv              - Set the extensions environment mode\n"
    "\nModule Discovery:\n"
    "  findall             - Attempts to detect environment and load all modules\n"
    "  findmodule          - Find the currently running module\n"
    "  loadmodules         - Find and loads symbols for all modules in the debug list\n"
    "\nData Parsing:\n"
    "  memorymap           - Prints the current memory map\n"
    "  hobs                - Enumerates the hand off blocks\n"
    "  protocols           - Lists the protocols from the protocol list.\n"
    "  handles             - Prints the handles list.\n"
    "  linkedlist          - Parses a UEFI style linked list of entries.\n"
    "  efierror            - Translates an EFI error code.\n"
    "  advlog              - Prints the advanced logger memory log.\n"
    "\nUEFI Debugger:\n"
    "  info                - Queries information about the UEFI debugger\n"
    "  modulebreak         - Sets a break on load for the provided module. e.g. 'shell'\n"
    "  readmsr             - Reads a MSR value (x86 only)\n"
    "  readvar             - Reads a UEFI variable\n"
    "  reboot              - Reboots the system\n"
    );

  EXIT_API ();

  return S_OK;
}

HRESULT CALLBACK
init (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  ULONG  TargetClass = 0;
  ULONG  TargetQual  = 0;
  ULONG  Mask;

  INIT_API ();

  UNREFERENCED_PARAMETER (args);

  dprintf ("Initializing UEFI Debugger Extension\n");
  g_ExtControl->GetDebuggeeType (&TargetClass, &TargetQual);
  if ((TargetClass == DEBUG_CLASS_KERNEL) && (TargetQual == DEBUG_KERNEL_EXDI_DRIVER)) {
    // Enabled the verbose flag in the output mask. This is required for .exdicmd
    // output.
    Client->GetOutputMask (&Mask);
    Client->SetOutputMask (Mask | DEBUG_OUTPUT_VERBOSE);

    dprintf ("EXDI Connection, scanning for images.\n");
    g_ExtControl->Execute (
                    DEBUG_OUTCTL_ALL_CLIENTS,
                    "!findall",
                    DEBUG_EXECUTE_DEFAULT
                    );
  }

  EXIT_API ();

  return S_OK;
}

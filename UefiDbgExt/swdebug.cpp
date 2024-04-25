/*++

Copyright (c) Microsoft Corporation.

SPDX-License-Identifier: BSD-2-Clause-Patent

Module Name:

    swdebug.cpp

Abstract:

    This file contains implementations specific to the UEFI software debugger.

--*/

#include "uefiext.h"

HRESULT CALLBACK
info (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  INIT_API ();

  g_ExtControl->Execute (
                  DEBUG_OUTCTL_ALL_CLIENTS,
                  ".exdicmd target:0:?",
                  DEBUG_EXECUTE_DEFAULT
                  );

  EXIT_API ();
  return S_OK;
}

HRESULT CALLBACK
modulebreak (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  CHAR  Command[512];

  INIT_API ();

  sprintf_s (Command, sizeof (Command), ".exdicmd target:0:b%s", args);
  g_ExtControl->Execute (
                  DEBUG_OUTCTL_ALL_CLIENTS,
                  Command,
                  DEBUG_EXECUTE_DEFAULT
                  );

  EXIT_API ();
  return S_OK;
}

HRESULT CALLBACK
readmsr (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  CHAR  Command[512];

  INIT_API ();

  if (strlen (args) == 0) {
    dprintf ("Must provide MSR index in HEX!");
  }

  sprintf_s (Command, sizeof (Command), ".exdicmd target:0:\"m%s\"", args);
  g_ExtControl->Execute (
                  DEBUG_OUTCTL_ALL_CLIENTS,
                  Command,
                  DEBUG_EXECUTE_DEFAULT
                  );

  EXIT_API ();
  return S_OK;
}

HRESULT CALLBACK
readvar (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  CHAR  Command[512];

  INIT_API ();

  if (strlen (args) == 0) {
    dprintf ("Must provide variable name!");
  }

  sprintf_s (Command, sizeof (Command), ".exdicmd target:*:\"v%s\"", args);
  g_ExtControl->Execute (
                  DEBUG_OUTCTL_ALL_CLIENTS,
                  Command,
                  DEBUG_EXECUTE_DEFAULT
                  );

  EXIT_API ();
  return S_OK;
}

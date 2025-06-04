/*++

    Copyright (c) Microsoft Corporation.

    SPDX-License-Identifier: BSD-2-Clause-Patent

Module Name:

    dbgexts.h

--*/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "uefispec.h"

//
// Define KDEXT_64BIT to make all wdbgexts APIs recognize 64 bit addresses
// It is recommended for extensions to use 64 bit headers from wdbgexts so
// the extensions could support 64 bit targets.
//
#define KDEXT_64BIT
#include <wdbgexts.h>
#include <dbgeng.h>

#pragma warning(disable:4201) // nonstandard extension used : nameless struct
#include <extsfns.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _PRINTF_DML_COLOR {
  Normal,
  Verbose,
  Warning,
  Err,
  Subdued,
  Header,
  Emphasized,
  Changed,
  ColorMax
} PRINTF_DML_COLOR, *PPRINTF_DML_COLOR;

VOID
PrintDml (
  __in PRINTF_DML_COLOR  Mask,
  __in PCSTR             Format,
  ...
  );

#define INIT_API()                             \
    HRESULT Status;                            \
    if ((Status = ExtQuery(Client)) != S_OK) return Status;

#define EXT_RELEASE(Unk) \
    ((Unk) != NULL ? ((Unk)->Release(), (Unk) = NULL) : NULL)

#define EXIT_API  ExtRelease

// Global variables initialized by query.
extern PDEBUG_CLIENT4    g_ExtClient;
extern PDEBUG_CONTROL    g_ExtControl;
extern PDEBUG_SYMBOLS2   g_ExtSymbols;
extern PDEBUG_REGISTERS  g_ExtRegisters;
extern ULONG             g_TargetMachine;

extern BOOL   Connected;
extern ULONG  TargetMachine;

HRESULT
ExtQuery (
  PDEBUG_CLIENT4  Client
  );

void
ExtRelease (
  void
  );

HRESULT
NotifyOnTargetAccessible (
  PDEBUG_CONTROL  Control
  );

#ifdef __cplusplus
}
#endif

/*++

    Copyright (c) Microsoft Corporation.

    SPDX-License-Identifier: BSD-2-Clause-Patent

Module Name:

    modules.cpp

Abstract:

    This file contains debug commands for enumerating UEFI modules and their
    symbols.

--*/

#include "uefiext.h"

//
// *******************************************************  External Functions
//

HRESULT CALLBACK
protocols (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  ULONG64  HeadAddress;
  ULONG64  Entry;
  GUID     ProtocolID;
  UINT64   Signature;
  UINT64   Index;
  ULONG    ProtListOffset;
  ULONG    NotifyListOffset;

  INIT_API ();

  // enumerate all protocol entries.
  HeadAddress = GetExpression ("&mProtocolDatabase");
  if (HeadAddress == NULL) {
    dprintf ("Failed to find mProtocolDatabase!\n");
    return ERROR_NOT_FOUND;
  }

  GetFieldOffset ("PROTOCOL_ENTRY", "Protocols", &ProtListOffset);
  GetFieldOffset ("PROTOCOL_ENTRY", "Notify", &NotifyListOffset);

  Index = 0;
  Entry = 0;
  while ((Entry = GetNextListEntry (HeadAddress, "PROTOCOL_ENTRY", "AllEntries", Entry)) != 0) {
    GetFieldValue (Entry, "PROTOCOL_ENTRY", "Signature", Signature);
    GetFieldValue (Entry, "PROTOCOL_ENTRY", "ProtocolID", ProtocolID);
    g_ExtControl->ControlledOutput (
                    DEBUG_OUTCTL_AMBIENT_DML,
                    DEBUG_OUTPUT_NORMAL,
                    "<exec cmd=\"dt (PROTOCOL_ENTRY)%I64x\">[%d]</exec> "
                    "%s "
                    "<exec cmd=\"!linkedlist %I64x PROTOCOL_INTERFACE ByProtocol\">Protocols</exec> "
                    "<exec cmd=\"!linkedlist %I64x PROTOCOL_NOTIFY Link\">Notify</exec>\n",
                    Entry,
                    Index++,
                    GuidToString (&ProtocolID),
                    Entry + ProtListOffset,
                    Entry + NotifyListOffset
                    );
  }

  EXIT_API ();
  return S_OK;
}

HRESULT CALLBACK
handles (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  ULONG64  HeadAddress;
  ULONG64  Entry;
  UINT64   Signature;
  ULONG    ProtListOffset;

  INIT_API ();

  UNREFERENCED_PARAMETER (args);

  HeadAddress = GetExpression ("&gHandleList");
  if (HeadAddress == NULL) {
    dprintf ("Failed to find gHandleList!\n");
    return ERROR_NOT_FOUND;
  }

  GetFieldOffset ("IHANDLE", "Protocols", &ProtListOffset);
  Entry = 0;
  while ((Entry = GetNextListEntry (HeadAddress, "IHANDLE", "AllHandles", Entry)) != 0) {
    GetFieldValue (Entry, "IHANDLE", "Signature", Signature);

    g_ExtControl->ControlledOutput (
                    DEBUG_OUTCTL_AMBIENT_DML,
                    DEBUG_OUTPUT_NORMAL,
                    "<exec cmd=\"dt (IHANDLE)%I64x\">%16I64x</exec> "
                    "<exec cmd=\"!linkedlist %I64x PROTOCOL_INTERFACE ByProtocol\">Protocols</exec>\n",
                    Entry,
                    Entry,
                    Entry + ProtListOffset
                    );
  }

  EXIT_API ();
  return S_OK;
}

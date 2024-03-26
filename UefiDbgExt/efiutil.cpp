/*++

    Copyright (c) Microsoft Corporation.

    SPDX-License-Identifier: BSD-2-Clause-Patent

Module Name:

    efiutil.cpp

Abstract:

    This file contains generic utility function for parsing EFI structures.

--*/

#include "uefiext.h"

UINT64
GetNextListEntry (
  IN ULONG64  Head,
  IN LPCSTR   Type,
  IN LPCSTR   Field,
  IN UINT64   Previous
  )
{
  ULONG    LinkOffset;
  ULONG64  LinkAddress;

  if (Head == 0) {
    dprintf ("Invalid list head!\n");
    return 0;
  }

  GetFieldOffset (Type, Field, &LinkOffset);

  if (Previous == 0) {
    GetFieldValue (Head, "_LIST_ENTRY", "ForwardLink", LinkAddress);
  } else {
    GetFieldValue (Previous + LinkOffset, "_LIST_ENTRY", "ForwardLink", LinkAddress);
  }

  if (LinkAddress == 0) {
    dprintf ("Invalid list link!\n");
    return 0;
  }

  if (LinkAddress == Head) {
    return 0;
  }

  return (LinkAddress - LinkOffset);
}

PCHAR
GuidToString (
  GUID  *Guid
  )
{
  static CHAR  GuidBuffer[40];

  sprintf_s (
    GuidBuffer,
    sizeof (GuidBuffer),
    "{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
    Guid->Data1,
    Guid->Data2,
    Guid->Data3,
    Guid->Data4[0],
    Guid->Data4[1],
    Guid->Data4[2],
    Guid->Data4[3],
    Guid->Data4[4],
    Guid->Data4[5],
    Guid->Data4[6],
    Guid->Data4[7]
    );

  return GuidBuffer;
}

ULONG
TokenizeArgs (
  PCSTR  args,
  PSTR   **Tokens
  )
{
  static CHAR  Strings[128];
  static CHAR  *TokenList[32];
  ULONG        Count;
  CHAR         *Curr;
  BOOLEAN      InToken;

  if (strlen (args) >= sizeof (Strings)) {
    dprintf ("Arguments too long for tokenizer!");
    return 0;
  }

  strcpy (Strings, args);

  Count   = 0;
  Curr    = &Strings[0];
  InToken = FALSE;
  while (*Curr != '\0') {
    if (*Curr == ' ') {
      InToken = FALSE;
      *Curr   = '\0';
    } else if (!InToken) {
      if (Count >= sizeof (TokenList)) {
        dprintf ("Too many tokens!");
        return 0;
      }

      TokenList[Count] = Curr;
      InToken          = TRUE;
      Count++;
    }

    Curr++;
  }

  if (Tokens != NULL) {
    *Tokens = TokenList;
  }

  return Count;
}

HRESULT CALLBACK
linkedlist (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  PSTR     *Tokens;
  CHAR     TokenCount;
  ULONG64  HeadAddr;
  ULONG64  Entry;
  CHAR     Command[256];

  INIT_API ();

  TokenCount = TokenizeArgs (args, &Tokens);
  if (TokenCount != 3) {
    dprintf ("Usage: !linkedlist <List Head> <Type> <Link Field>");
    return ERROR_INVALID_PARAMETER;
  }

  if (GetExpressionEx (Tokens[0], &HeadAddr, NULL) == FALSE) {
    dprintf ("Invalid list head!");
    return ERROR_INVALID_PARAMETER;
  }

  while ((Entry = GetNextListEntry (HeadAddr, Tokens[1], Tokens[2], Entry)) != 0) {
    sprintf_s (&Command[0], sizeof (Command), "dt (%s)%I64x", Tokens[1], Entry);
    g_ExtControl->Execute (
                    DEBUG_OUTCTL_ALL_CLIENTS,
                    &Command[0],
                    DEBUG_EXECUTE_DEFAULT
                    );
  }

  EXIT_API ();
  return S_OK;
}

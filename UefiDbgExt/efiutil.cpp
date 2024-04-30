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

PCSTR
ErrorLevelToString (
  UINT32  ErrorLevel
  )
{
  switch (ErrorLevel) {
    case 0x00000001:
      return "INIT";
    case 0x00000002:
      return "WARN";
    case 0x00000004:
      return "LOAD";
    case 0x00000008:
      return "FS";
    case 0x00000010:
      return "POOL";
    case 0x00000020:
      return "PAGE";
    case 0x00000040:
      return "INFO";
    case 0x00000080:
      return "DISPATCH";
    case 0x00000100:
      return "VARIABLE";
    case 0x00000200:
      return "SMI";
    case 0x00000400:
      return "BM";
    case 0x00001000:
      return "BLKIO";
    case 0x00004000:
      return "NET";
    case 0x00010000:
      return "UNDI";
    case 0x00020000:
      return "LDFILE";
    case 0x00080000:
      return "EVENT";
    case 0x00100000:
      return "GCD";
    case 0x00200000:
      return "CACHE";
    case 0x00400000:
      return "VERBOSE";
    case 0x00800000:
      return "MANAGEABILITY";
    case 0x80000000:
      return "ERROR";
    default:
      return "UNK";
  }
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

  strcpy_s (Strings, sizeof (Strings), args);

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
  ULONG    TokenCount;
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

  Entry = 0;
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

HRESULT CALLBACK
efierror (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  ULONG64        Error;
  ULONG64        ErrorOffset;
  CONST ULONG64  ErrorBit = 0x8000000000000000ULL;
  PCSTR          String;
  CONST PCSTR    Codes[] = {
    "EFI_SUCCESS",                // 0
    "EFI_WARN_UNKNOWN_GLYPH",     // 1
    "EFI_WARN_DELETE_FAILURE",    // 2
    "EFI_WARN_WRITE_FAILURE",     // 3
    "EFI_WARN_BUFFER_TOO_SMALL",  // 4
    "EFI_WARN_STALE_DATA",        // 5
    "EFI_WARN_FILE_SYSTEM"        // 6
  };

  CONST PCSTR  ErrorCodes[] = {
    "UNKNOWN",                    // 0
    "EFI_LOAD_ERROR",             // 1
    "EFI_INVALID_PARAMETER",      // 2
    "EFI_UNSUPPORTED",            // 3
    "EFI_BAD_BUFFER_SIZE",        // 4
    "EFI_BUFFER_TOO_SMALL",       // 5
    "EFI_NOT_READY",              // 6
    "EFI_DEVICE_ERROR",           // 7
    "EFI_WRITE_PROTECTED",        // 8
    "EFI_OUT_OF_RESOURCES",       // 9
    "EFI_VOLUME_CORRUPTED",       // 10
    "EFI_VOLUME_FULL",            // 11
    "EFI_NO_MEDIA",               // 12
    "EFI_MEDIA_CHANGED",          // 13
    "EFI_NOT_FOUND",              // 14
    "EFI_ACCESS_DENIED",          // 15
    "EFI_NO_RESPONSE",            // 16
    "EFI_NO_MAPPING",             // 17
    "EFI_TIMEOUT",                // 18
    "EFI_NOT_STARTED",            // 19
    "EFI_ALREADY_STARTED",        // 20
    "EFI_ABORTED",                // 21
    "EFI_ICMP_ERROR",             // 22
    "EFI_TFTP_ERROR",             // 23
    "EFI_PROTOCOL_ERROR",         // 24
    "EFI_INCOMPATIBLE_VERSION"    // 25
    "EFI_SECURITY_VIOLATION",     // 26
    "EFI_CRC_ERROR",              // 27
    "EFI_END_OF_MEDIA",           // 28
    "UNKNOWN",                    // 29
    "UNKNOWN",                    // 30
    "EFI_END_OF_FILE",            // 31
    "EFI_INVALID_LANGUAGE",       // 32
    "EFI_COMPROMISED_DATA",       // 33
    "UNKNOWN",                    // 34
    "EFI_HTTP_ERROR"              // 35
  };

  INIT_API ();

  if (GetExpressionEx (args, &Error, NULL) == FALSE) {
    dprintf ("Must provide error code or variable!");
    return ERROR_INVALID_PARAMETER;
  }

  String = "UNKNOWN";
  if ((Error & ErrorBit) != 0) {
    ErrorOffset = Error & ~ErrorBit;
    if (ErrorOffset < ARRAYSIZE (ErrorCodes)) {
      String = ErrorCodes[ErrorOffset];
    }
  } else if (Error < ARRAYSIZE (Codes)) {
    String = Codes[Error];
  }

  dprintf ("0x%I64x = %s\n", Error, String);

  EXIT_API ();
  return S_OK;
}

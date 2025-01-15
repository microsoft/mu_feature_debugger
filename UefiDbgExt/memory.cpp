/*++

    Copyright (c) Microsoft Corporation.

    SPDX-License-Identifier: BSD-2-Clause-Patent

Module Name:

    memory.cpp

Abstract:

    This file contains debug commands for memory operations.

--*/

#include "uefiext.h"

//
// **************************************************************  Definitions
//

#pragma pack (push, 1)
typedef struct {
  UINT32    Signature;          // Signature
  UINT8     MajorVersion;       // Major version of advanced logger message structure
  UINT8     MinorVersion;       // Minor version of advanced logger message structure
  UINT32    DebugLevel;         // Debug Level
  UINT64    TimeStamp;          // Time stamp
  UINT16    Phase;              // Boot phase that produced this message entry
  UINT16    MessageLen;         // Number of bytes in Message
  UINT16    MessageOffset;      // Offset of Message from start of structure,
                                // used to calculate the address of the Message
  // CHAR      MessageText[];      // Message Text
} ADVANCED_LOGGER_MESSAGE_ENTRY_V2;
#pragma pack (pop)

PCSTR  MemoryTypeString[] = {
  "EfiReservedMemoryType",
  "EfiLoaderCode",
  "EfiLoaderData",
  "EfiBootServicesCode",
  "EfiBootServicesData",
  "EfiRuntimeServicesCode",
  "EfiRuntimeServicesData",
  "EfiConventionalMemory",
  "EfiUnusableMemory",
  "EfiACPIReclaimMemory",
  "EfiACPIMemoryNVS",
  "EfiMemoryMappedIO",
  "EfiMemoryMappedIOPortSpace",
  "EfiPalCode",
  "EfiPersistentMemory"
};

#define MEMORY_TYPE_COUNT  (sizeof(MemoryTypeString) / sizeof(MemoryTypeString[0]))

PSTR  HobTypes[] = {
  NULL,                                     // 0x0000
  "EFI_HOB_HANDOFF_INFO_TABLE",             // 0x0001
  "EFI_HOB_MEMORY_ALLOCATION",              // 0x0002
  "EFI_HOB_RESOURCE_DESCRIPTOR",            // 0x0003
  "EFI_HOB_GUID_TYPE",                      // 0x0004
  "EFI_HOB_FIRMWARE_VOLUME",                // 0x0005
  "EFI_HOB_CPU",                            // 0x0006
  "EFI_HOB_MEMORY_POOL",                    // 0x0007
  NULL,                                     // 0x0008
  "EFI_HOB_FIRMWARE_VOLUME2",               // 0x0009
  NULL,                                     // 0x000A
  "EFI_HOB_UEFI_CAPSULE",                   // 0x000B
  "EFI_HOB_FIRMWARE_VOLUME3"                // 0x000C
};

#define HOB_TYPE_COUNT  (sizeof(HobTypes) / sizeof(HobTypes[0]))

PCSTR  PhaseStrings[] = {
  "UNSPEC",
  "SEC",
  "PEI",
  "PEI64",
  "DXE",
  "RT",
  "MmCore",
  "MM",
  "SmmCore",
  "SMM",
  "TFA",
  "CNT",
};

#define PHASE_COUNT  (sizeof(PhaseStrings) / sizeof(PhaseStrings[0]))

//
// *******************************************************  External Functions
//

HRESULT CALLBACK
memorymap (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  ULONG64  HeadAddress;
  ULONG64  MemoryEntry;
  ULONG    Type;
  UINT64   Start;
  UINT64   End;
  UINT64   Pages;
  UINT64   Attribute;
  ULONG64  TotalMemory                 = 0;
  ULONG64  TypeSize[MEMORY_TYPE_COUNT] = { 0 };

  INIT_API ();

  UNREFERENCED_PARAMETER (args);

  if (gUefiEnv != DXE) {
    dprintf ("Only supported for DXE!\n");
    return ERROR_NOT_SUPPORTED;
  }

  HeadAddress = GetExpression ("&gMemoryMap");
  if (HeadAddress == NULL) {
    dprintf ("Failed to find gMemoryMap!\n");
    return ERROR_NOT_FOUND;
  }

  dprintf ("    Start             End               Pages             Attributes        MemoryType   \n");
  dprintf ("-------------------------------------------------------------------------------------------------------\n");
  MemoryEntry = 0;
  while ((MemoryEntry = GetNextListEntry (HeadAddress, "MEMORY_MAP", "Link", MemoryEntry)) != 0) {
    GetFieldValue (MemoryEntry, "MEMORY_MAP", "Type", Type);
    GetFieldValue (MemoryEntry, "MEMORY_MAP", "Start", Start);
    GetFieldValue (MemoryEntry, "MEMORY_MAP", "End", End);
    GetFieldValue (MemoryEntry, "MEMORY_MAP", "Attribute", Attribute);
    Pages = ((End + 1) - Start) / PAGE_SIZE;

    dprintf (
      "    %016I64x  %016I64x  %16I64x  %016I64x  %-2d (%s)\n",
      Start,
      End,
      Pages,
      Attribute,
      Type,
      Type < MEMORY_TYPE_COUNT ? MemoryTypeString[Type] : "Unknown"
      );

    //
    // Memory size tracking.
    //

    TotalMemory += (End - Start + 1);
    if (Type < MEMORY_TYPE_COUNT) {
      TypeSize[Type] += (End - Start + 1);
    }
  }

  dprintf ("-------------------------------------------------------------------------------------------------------\n");
  for (Type = 0; Type < MEMORY_TYPE_COUNT; Type++) {
    dprintf ("    %-30s %16I64x\n", MemoryTypeString[Type], TypeSize[Type]);
  }

  dprintf ("\n    %-30s %16I64x\n", "Total", TotalMemory);

  dprintf ("-------------------------------------------------------------------------------------------------------\n");

  EXIT_API ();
  return S_OK;
}

HRESULT CALLBACK
hobs (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  ULONG64  HobAddr;
  UINT16   HobType;
  UINT16   HobLength;
  PSTR     TypeString;

  INIT_API ();

  //
  // Collect the hobs in the environment specific way.
  //

  if (gUefiEnv == DXE) {
    if (GetExpressionEx (args, &HobAddr, &args) == FALSE) {
      HobAddr = GetTableAddress (HobList);
    }

    if (HobAddr == 0) {
      dprintf ("Hob list not found!\n");
      return ERROR_NOT_FOUND;
    }

    dprintf ("Enumerating Hob list at 0x%I64x\n\n", HobAddr);
    dprintf ("    Address             Length  Type\n");
    dprintf ("-------------------------------------------------------------------\n");
    do {
      GetFieldValue (HobAddr, "EFI_HOB_GENERIC_HEADER", "HobType", HobType);
      GetFieldValue (HobAddr, "EFI_HOB_GENERIC_HEADER", "HobLength", HobLength);

      dprintf (
        "    %016I64x    %04x    (0x%x) - ",
        HobAddr,
        HobLength,
        HobType,
        HobType < HOB_TYPE_COUNT ? HobTypes[HobType] : "UNKNOWN"
        );

      //
      // Handle enumerations if possible
      //

      TypeString = NULL;
      if (HobType < HOB_TYPE_COUNT) {
        TypeString = HobTypes[HobType];
      }

      if (TypeString == NULL) {
        TypeString = "EFI_HOB_GENERIC_HEADER";
      }

      g_ExtControl->ControlledOutput (
                      DEBUG_OUTCTL_AMBIENT_DML,
                      DEBUG_OUTPUT_NORMAL,
                      "<exec cmd=\"dt %s %016I64x\">%s</exec> ",
                      TypeString,
                      HobAddr,
                      TypeString
                      );

      dprintf ("\n");

      HobAddr += HobLength;
    } while ((HobType != 0xFFFF) && (HobLength != 0));
  } else {
    dprintf ("Not supported for this environment!\n");
    return ERROR_NOT_SUPPORTED;
  }

  EXIT_API ();
  return S_OK;
}

HRESULT CALLBACK
advlog (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  ULONG64                           InfoAddress;
  ULONG64                           EntryAddress;
  ULONG64                           EndAddress;
  ULONG                             LogBufferSize;
  UINT16                            Version;
  ULONG                             BytesRead;
  HRESULT                           Result;
  CHAR                              *LogBuffer = NULL;
  ULONG                             Offset;
  ULONG                             End;
  ADVANCED_LOGGER_MESSAGE_ENTRY_V2  *Entry;

  // NOTE: This implementation is a crude first past, The following should be done
  // in the future.
  //
  // 1. Handle circular buffer.
  // 2. Handle interleaved multipart messages.
  // 3. More robust error checking.
  // 4. Print metadata and allow filtering.
  //

  INIT_API ();

  // If no valid input address was give, find the symbol.
  if (GetExpressionEx (args, &InfoAddress, &args) == FALSE) {
    InfoAddress = GetExpression ("mLoggerInfo");
    if (InfoAddress == NULL) {
      dprintf ("Failed to find mLoggerInfo!\n");
      Result = ERROR_NOT_FOUND;
      goto Exit;
    }

    if (!ReadPointer (InfoAddress, &InfoAddress)) {
      dprintf ("Failed to read mLoggerInfo!\n");
      Result = ERROR_NOT_FOUND;
      goto Exit;
    }
  }

  if (InfoAddress == NULL) {
    dprintf ("Logger info is NULL!\n");
    Result = ERROR_NOT_FOUND;
    goto Exit;
  }

  GetFieldValue (InfoAddress, "ADVANCED_LOGGER_INFO", "Version", Version);
  GetFieldValue (InfoAddress, "ADVANCED_LOGGER_INFO", "LogBufferSize", LogBufferSize);

  g_ExtControl->ControlledOutput (
                  DEBUG_OUTCTL_AMBIENT_DML,
                  DEBUG_OUTPUT_NORMAL,
                  "Header:   <exec cmd=\"dt ADVANCED_LOGGER_INFO %016I64x\">%x</exec>\n",
                  InfoAddress,
                  InfoAddress
                  );

  dprintf ("Version:  %d\n", Version);
  dprintf ("Size:     0x%x bytes\n", LogBufferSize);

  if (LogBufferSize == 0) {
    dprintf ("Bad log buffer size!\n");
    Result = ERROR_NOT_SUPPORTED;
    goto Exit;
  }

  if (Version == 4) {
    GetFieldValue (InfoAddress, "ADVANCED_LOGGER_INFO", "LogBuffer", EntryAddress);
    GetFieldValue (InfoAddress, "ADVANCED_LOGGER_INFO", "LogCurrent", EndAddress);
  } else if (Version == 5) {
    GetFieldValue (InfoAddress, "ADVANCED_LOGGER_INFO", "LogBufferOffset", EntryAddress);
    EntryAddress += InfoAddress;
    GetFieldValue (InfoAddress, "ADVANCED_LOGGER_INFO", "LogCurrentOffset", EndAddress);
    EndAddress += InfoAddress;
  } else {
    dprintf ("\nVersion not implemented in debug extension!\n");
    Result = ERROR_NOT_SUPPORTED;
    goto Exit;
  }

  if (EndAddress < EntryAddress) {
    dprintf ("Looped logs not yet implemented in extension!\n");
    Result = ERROR_NOT_SUPPORTED;
    goto Exit;
  } else {
    // non-loop optimization, only download through the last message.
    LogBufferSize = min (LogBufferSize, (ULONG)(EndAddress - InfoAddress));
  }

  LogBuffer = (CHAR *)malloc (LogBufferSize);

  // Output() forces IO flush to inform user.
  g_ExtControl->Output (DEBUG_OUTPUT_NORMAL, "Reading log (0x%x bytes) ... \n", LogBufferSize);
  ReadMemory (InfoAddress, LogBuffer, LogBufferSize, &BytesRead);
  if (BytesRead != LogBufferSize) {
    dprintf ("Failed to read log memory!\n");
    Result = ERROR_BAD_LENGTH;
    goto Exit;
  }

  Offset = (ULONG)(EntryAddress - InfoAddress);
  End    = (ULONG)(EndAddress - InfoAddress);

  dprintf ("\n------------------------------------------------------------------------------\n");
  BOOLEAN  PrevNL = TRUE;

  while (Offset < End) {
    Entry = (ADVANCED_LOGGER_MESSAGE_ENTRY_V2 *)(LogBuffer + Offset);
    if (Entry->Signature != 0x324d4c41) {
      dprintf ("\nBad message signature!!\n");
      break;
    }

    ULONG  StringEnd = Offset + Entry->MessageOffset + Entry->MessageLen;
    CHAR   Temp      = LogBuffer[StringEnd];
    LogBuffer[StringEnd] = 0;

    if (PrevNL) {
      dprintf (
        "%-8s| %-8s| ",
        (Entry->Phase < PHASE_COUNT ? PhaseStrings[Entry->Phase] : "UNK"),
        ErrorLevelToString (Entry->DebugLevel)
        );
    }

    dprintf ("%s", LogBuffer + Offset + Entry->MessageOffset);
    PrevNL = (LogBuffer[StringEnd - 1] == '\n');

    LogBuffer[StringEnd] = Temp;

    Offset = ALIGN_UP (Offset + Entry->MessageOffset + Entry->MessageLen, 8);
  }

  dprintf ("\n------------------------------------------------------------------------------\n");

  Result = S_OK;

Exit:
  if (LogBuffer != NULL) {
    free (LogBuffer);
  }

  EXIT_API ();
  return Result;
}

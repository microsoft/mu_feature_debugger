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

HRESULT
FindModuleBackwards (
  ULONG64  Address
  )
{
  ULONG64        MinAddress;
  CHAR           Command[512];
  ULONG64        MaxSize;
  ULONG32        Check;
  CONST ULONG32  Magic    = 0x5A4D;     // MZ
  CONST ULONG32  ElfMagic = 0x464C457F; // 0x7F_ELF
  ULONG          BytesRead;
  HRESULT        Result;
  ULONG64        Base;

  MaxSize = 0x400000;   // 4 Mb
  Address = PAGE_ALIGN_DOWN (Address);
  if (Address > MaxSize) {
    MinAddress = Address - MaxSize;
  } else {
    MinAddress = 0;
  }

  // Check this hasn't already be loaded.
  Result = g_ExtSymbols->GetModuleByOffset (Address, 0, NULL, &Base);
  if (Result == S_OK) {
    dprintf ("Already loaded module at %llx\n", Base);
    return Result;
  }

  Result = ERROR_NOT_FOUND;
  for ( ; Address >= MinAddress; Address -= PAGE_SIZE) {
    Check = 0;
    ReadMemory (Address, &Check, sizeof (Check), &BytesRead);
    if (BytesRead != sizeof (Check)) {
      break;
    }

    if ((Check & 0xFFFF) == Magic) {
      sprintf_s (&Command[0], sizeof (Command), ".imgscan /l /r %I64x %I64x", Address, Address + 0xFFF);
      g_ExtControl->Execute (
                      DEBUG_OUTCTL_ALL_CLIENTS,
                      &Command[0],
                      DEBUG_EXECUTE_DEFAULT
                      );

      Result = S_OK;
      break;
    } else if (Check == ElfMagic) {
      sprintf_s (&Command[0], sizeof (Command), "!uefiext.elf %I64x", Address);
      g_ExtControl->Execute (
                      DEBUG_OUTCTL_ALL_CLIENTS,
                      &Command[0],
                      DEBUG_EXECUTE_DEFAULT
                      );

      Result = S_OK;
      break;
    }
  }

  return Result;
}

HRESULT CALLBACK
loadmodules (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  ULONG64  HeaderAddress;
  UINT32   TableSize;
  ULONG64  Table;
  ULONG64  Entry;
  ULONG64  NormalImage;
  ULONG64  ImageProtocol;
  UINT64   ImageBase;
  ULONG    Index;
  CHAR     Command[512];
  ULONG64  Base;

  INIT_API ();

  UNREFERENCED_PARAMETER (args);

  //
  // TODO: Add support for PEI & MM
  //

  if (gUefiEnv != DXE) {
    dprintf ("Only supported for DXE!\n");
    return ERROR_NOT_SUPPORTED;
  }

  HeaderAddress = GetExpression ("&mDebugInfoTableHeader");
  if (HeaderAddress == NULL) {
    dprintf ("Failed to find mDebugInfoTableHeader!\n");
    return ERROR_NOT_FOUND;
  }

  GetFieldValue (HeaderAddress, "EFI_DEBUG_IMAGE_INFO_TABLE_HEADER", "TableSize", TableSize);
  GetFieldValue (HeaderAddress, "EFI_DEBUG_IMAGE_INFO_TABLE_HEADER", "EfiDebugImageInfoTable", Table);
  if ((Table == NULL) || (TableSize == 0)) {
    dprintf ("Debug table is empty!\n");
    return ERROR_NOT_FOUND;
  }

  if (TableSize <= 1) {
    dprintf ("Debug info array is empty.\n");
  }

  // Skip the 0-index to avoid reloading DxeCore. There is probably a better way to do this.
  for (Index = 1; Index < TableSize; Index++) {
    Entry = Table + (Index * GetTypeSize ("EFI_DEBUG_IMAGE_INFO"));
    GetFieldValue (Entry, "EFI_DEBUG_IMAGE_INFO", "NormalImage", NormalImage);
    if (NormalImage == NULL) {
      dprintf ("Skipping missing normal info!\n");
      continue;
    }

    GetFieldValue (NormalImage, "EFI_DEBUG_IMAGE_INFO_NORMAL", "LoadedImageProtocolInstance", ImageProtocol);
    if (ImageProtocol == NULL) {
      dprintf ("Skipping missing loaded image protocol!\n");
      continue;
    }

    GetFieldValue (ImageProtocol, "EFI_LOADED_IMAGE_PROTOCOL", "ImageBase", ImageBase);

    // Check this hasn't already be loaded.
    if ((g_ExtSymbols->GetModuleByOffset (ImageBase, 0, NULL, &Base) == S_OK) &&
        (ImageBase == Base))
    {
      continue;
    }

    sprintf_s (Command, sizeof (Command), ".imgscan /l /r %I64x (%I64x + 0xFFF)", ImageBase, ImageBase);
    g_ExtControl->Execute (
                    DEBUG_OUTCTL_ALL_CLIENTS,
                    Command,
                    DEBUG_EXECUTE_DEFAULT
                    );
  }

  EXIT_API ();
  return S_OK;
}

HRESULT CALLBACK
findmodule (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  ULONG64  Address;
  HRESULT  Result;

  INIT_API ();

  if (strlen (args) == 0) {
    args = "@$ip";
  }

  Address = GetExpression (args);
  if ((Address == 0) || (Address == (-1))) {
    dprintf ("Invalid address!\n");
    dprintf ("Usage: !uefiext.findmodule [Address]\n");
    return ERROR_INVALID_PARAMETER;
  }

  Result = FindModuleBackwards (Address);

  EXIT_API ();
  return Result;
}

HRESULT CALLBACK
findall (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  HRESULT  Result;
  ULONG64  BsPtrAddr;
  ULONG64  BsTableAddr;

  INIT_API ();

  if (gUefiEnv != DXE) {
    dprintf ("Only supported for DXE!\n");
    return ERROR_NOT_SUPPORTED;
  }

  //
  // First find the current module
  //

  Result = FindModuleBackwards (GetExpression ("@$ip"));
  if (Result != S_OK) {
    return Result;
  }

  //
  // Find the core module. This might be the same as the executing one.
  //

  BsPtrAddr = GetExpression ("gBS");
  if (BsPtrAddr == NULL) {
    dprintf ("Failed to find boot services table pointer!\n");
    return ERROR_NOT_FOUND;
  }

  if (!ReadPointer (BsPtrAddr, &BsTableAddr)) {
    dprintf ("Failed to find boot services table!\n");
    return ERROR_NOT_FOUND;
  }

  Result = FindModuleBackwards (BsTableAddr);
  if (Result != S_OK) {
    return Result;
  }

  g_ExtControl->Execute (
                  DEBUG_OUTCTL_ALL_CLIENTS,
                  "ld *Core",
                  DEBUG_EXECUTE_DEFAULT
                  );

  //
  // Load all the other modules.
  //

  Result = loadmodules (Client, "");

  EXIT_API ();
  return S_OK;
}

#pragma pack (push, 1)
typedef struct _ELF_HEADER_64 {
  unsigned char    e_ident[16]; // 0x74 + ELF
  UINT16           e_type;
  UINT16           e_machine;
  UINT32           e_version;
  UINT64           e_entry;
  UINT64           e_phoff;
  UINT64           e_shoff;
  UINT32           e_flags;
  UINT16           e_ehsize;
  UINT16           e_phentsize;
  UINT16           e_phnum;
  UINT16           e_shentsize;
  UINT16           e_shnum;
  UINT16           e_shstrndx;
} ELF_HEADER_64;
C_ASSERT (sizeof (ELF_HEADER_64) == 64);

typedef struct _ELF_SECTION_64 {
  UINT32    sh_name;
  UINT16    e_type;
  UINT16    e_machine;
  UINT32    e_version;
  UINT64    e_entry;
  UINT64    e_phoff;
  UINT64    e_shoff;
  UINT32    e_flags;
  UINT16    e_ehsize;
  UINT16    e_phentsize;
  UINT16    e_phnum;
  UINT16    e_shentsize;
  UINT16    e_shnum;
  UINT16    e_shstrndx;
} ELF_SECTION_64;

#pragma pack (pop)

HRESULT CALLBACK
elf (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  ULONG64         Address;
  ELF_HEADER_64   Header = { 0 };
  ELF_SECTION_64  *Section;
  ULONG           BytesRead = 0;

  INIT_API ();

  if (strlen (args) == 0) {
    dprintf ("Usage: !uefiext.elf [Address]\n");
    return ERROR_INVALID_PARAMETER;
  }

  Address = GetExpression (args);
  if ((Address == 0) || (Address == (-1))) {
    dprintf ("Invalid address!\n");
    dprintf ("Usage: !uefiext.elf [Address]\n");
    return ERROR_INVALID_PARAMETER;
  }

  ReadMemory (Address, &Header, sizeof (Header), &BytesRead);
  if (BytesRead != sizeof (Header)) {
    dprintf ("Failed to read header!\n");
    return ERROR_BAD_ARGUMENTS;
  }

  if ((Header.e_ident[0] != 0x7F) || (Header.e_ident[1] != 'E') || (Header.e_ident[2] != 'L') || (Header.e_ident[3] != 'F')) {
    dprintf ("Invalid ELF header! Magic did not match.\n");
    return ERROR_INVALID_DATA;
  }

  dprintf ("ELF Header @ %llx\n", Address);
  dprintf ("------------------------------------\n");
  dprintf ("Type                     0x%x\n", Header.e_type);
  dprintf ("Machine                  0x%x\n", Header.e_machine);
  dprintf ("Version                  0x%x\n", Header.e_version);
  dprintf ("Entry                    0x%llx\n", Header.e_entry);
  dprintf ("Program Table Offset     0x%llx\n", Header.e_phoff);
  dprintf ("Section Table Offset     0x%llx\n", Header.e_shoff);
  dprintf ("Flags                    0x%x\n", Header.e_flags);
  dprintf ("Header Size              0x%x\n", Header.e_ehsize);
  dprintf ("Program Header Size      0x%x\n", Header.e_phentsize);
  dprintf ("Program Header Num       0x%x\n", Header.e_phnum);
  dprintf ("Section Header Size      0x%x\n", Header.e_shentsize);
  dprintf ("Section Header Num       0x%x\n", Header.e_shnum);
  dprintf ("Section Names Index      0x%x\n", Header.e_shstrndx);
  dprintf ("------------------------------------\n\n");

  // Print sections.
  Section = (ELF_SECTION_64 *)(Address + Header.e_phoff);

  EXIT_API ();
  return S_OK;
}

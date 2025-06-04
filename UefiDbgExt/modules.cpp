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

HRESULT
loadmodules (
  ULONG64  SystemTableAddr
  )
{
  UINT32                       TableSize;
  ULONG64                      Table;
  EFI_DEBUG_IMAGE_INFO         *Entry;
  EFI_DEBUG_IMAGE_INFO_NORMAL  *NormalImage;
  EFI_LOADED_IMAGE_PROTOCOL    *ImageProtocol;
  UINT64                       ImageBase;
  ULONG                        Index;
  CHAR                         Command[512];
  ULONG64                      Base;
  ULONG                        BytesRead = 0;

  //
  // TODO: Add support for PEI & MM
  //

  EFI_SYSTEM_TABLE                   SystemTable;
  EFI_CONFIGURATION_TABLE            *ConfigTable;
  GUID                               DebugImageInfoTableGuid    = EFI_DEBUG_IMAGE_INFO_TABLE_GUID;
  EFI_DEBUG_IMAGE_INFO_TABLE_HEADER  *DebugImageInfoTableHeader = NULL;

  // Read the EFI_SYSTEM_TABLE structure from the provided address
  if (!ReadMemory (SystemTableAddr, &SystemTable, sizeof (SystemTable), &BytesRead) || (BytesRead != sizeof (SystemTable))) {
    dprintf ("Failed to read EFI_SYSTEM_TABLE at %llx\n", SystemTableAddr);
    return ERROR_NOT_FOUND;
  }

  // Iterate through the configuration tables to find the debug image info table
  ConfigTable = SystemTable.ConfigurationTable;
  for (UINT64 i = 0; i < SystemTable.NumberOfTableEntries; i++) {
    EFI_CONFIGURATION_TABLE  CurrentTable;
    if (!ReadMemory ((ULONG64)&ConfigTable[i], &CurrentTable, sizeof (CurrentTable), &BytesRead) || (BytesRead != sizeof (CurrentTable))) {
      dprintf ("Failed to read configuration table entry at index %llu\n", i);
      continue;
    }

    if (memcmp (&CurrentTable.VendorGuid, &DebugImageInfoTableGuid, sizeof (GUID)) == 0) {
      DebugImageInfoTableHeader = (EFI_DEBUG_IMAGE_INFO_TABLE_HEADER *)CurrentTable.VendorTable;
      break;
    }
  }

  if (DebugImageInfoTableHeader == NULL) {
    dprintf ("Failed to locate EFI_DEBUG_IMAGE_INFO_TABLE_HEADER in configuration tables\n");
    return ERROR_NOT_FOUND;
  }

  // Read the debug image info table header
  if (!ReadMemory ((ULONG64)&DebugImageInfoTableHeader->TableSize, &TableSize, sizeof (TableSize), &BytesRead) || (BytesRead != sizeof (TableSize))) {
    dprintf ("Failed to read EFI_DEBUG_IMAGE_INFO_TABLE_HEADER at %llx\n", (ULONG64)DebugImageInfoTableHeader);
    return ERROR_NOT_FOUND;
  }

  if (!ReadMemory ((ULONG64)&DebugImageInfoTableHeader->EfiDebugImageInfoTable, &Table, sizeof (Table), &BytesRead) || (BytesRead != sizeof (Table))) {
    dprintf ("Failed to read EfiDebugImageInfoTable pointer\n");
    return ERROR_NOT_FOUND;
  }

  if ((Table == NULL) || (TableSize == 0)) {
    dprintf ("Debug image info table is empty!\n");
    return ERROR_NOT_FOUND;
  }

  // Iterate through the debug image info table entries
  for (Index = 0; Index < TableSize; Index++) {
    Entry = (EFI_DEBUG_IMAGE_INFO *)(Table + (Index * sizeof (EFI_DEBUG_IMAGE_INFO)));
    if (!ReadMemory ((ULONG64)&Entry->NormalImage, &NormalImage, sizeof (NormalImage), &BytesRead) || (BytesRead != sizeof (NormalImage))) {
      dprintf ("Failed to read debug image info entry at index %lu\n", Index);
      continue;
    }

    if (NormalImage == NULL) {
      dprintf ("Skipping missing normal image info at index %lu\n", Index);
      continue;
    }

    if (!ReadMemory ((ULONG64)&NormalImage->LoadedImageProtocolInstance, &ImageProtocol, sizeof (ImageProtocol), &BytesRead) || (BytesRead != sizeof (ImageProtocol))) {
      dprintf ("Failed to read loaded image protocol instance at index %lu\n", Index);
      continue;
    }

    if (ImageProtocol == NULL) {
      dprintf ("Skipping missing loaded image protocol at index %lu\n", Index);
      continue;
    }

    if (!ReadMemory ((ULONG64)&ImageProtocol->ImageBase, &ImageBase, sizeof (ImageBase), &BytesRead) || (BytesRead != sizeof (ImageBase))) {
      dprintf ("Failed to read image base at index %lu\n", Index);
      continue;
    }

    // Check if the module is already loaded
    if ((g_ExtSymbols->GetModuleByOffset (ImageBase, 0, NULL, &Base) == S_OK) && (ImageBase == Base)) {
      dprintf ("Module at %llx is already loaded\n", ImageBase);
      continue;
    }

    dprintf ("Loading module at %llx\n", ImageBase);
    sprintf_s (Command, sizeof (Command), ".imgscan /l /r %I64x (%I64x + 0xFFF)", ImageBase, ImageBase);
    g_ExtControl->Execute (
                    DEBUG_OUTCTL_ALL_CLIENTS,
                    Command,
                    DEBUG_EXECUTE_DEFAULT
                    );
  }

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
  ULONG64  SystemPtrAddr;
  ULONG64  SystemTableAddr;
  ULONG64  Signature            = 0;
  ULONG    BytesRead            = 0;
  ULONG64  SystemTableSignature = (('I') | (static_cast<ULONG64>('B') << 8) | (static_cast<ULONG64>('I') << 16) | (static_cast<ULONG64>(' ') << 24) | (static_cast<ULONG64>('S') << 32) | (static_cast<ULONG64>('Y') << 40) | (static_cast<ULONG64>('S') << 48) | (static_cast<ULONG64>('T') << 56));
  PSTR     Response;

  INIT_API ();

  if ((gUefiEnv != DXE) && (gUefiEnv != RUST)) {
    dprintf ("Only supported for DXE and Rust!\n");
    return ERROR_NOT_SUPPORTED;
  }

  //
  // First find the current module. We only do this to see if we are in the core to find the system table pointer
  // symbols. If we are not in the core, we will ask the monitor for the system table pointer address, failing that
  // we will scan memory for the EFI_SYSTEM_TABLE_SIGNATURE, per UEFI spec. The C DXE environment does not have the
  // monitor command, so relies on the core symbols having been loaded at the initial breakpoint (or us being broken
  // into the core now)
  //

  FindModuleBackwards (GetExpression ("@$ip"));

  g_ExtControl->Execute (
                  DEBUG_OUTCTL_ALL_CLIENTS,
                  "ld *ore*",
                  DEBUG_EXECUTE_DEFAULT
                  );

  //
  // Find the system table pointer, which we may not find if we are not in the core module
  //

  if (gUefiEnv == DXE) {
    SystemPtrAddr = GetExpression ("mDebugTable");
    if (!ReadPointer (SystemPtrAddr, &SystemPtrAddr)) {
      dprintf ("Failed to read memory at %llx to get system table from ptr\n", SystemPtrAddr);
      return ERROR_NOT_FOUND;
    }
  } else if (gUefiEnv == RUST) {
    Response      = MonitorCommandWithOutput (Client, "system_table_ptr", 0);
    SystemPtrAddr = strtoull (Response, NULL, 16);

    if (SystemPtrAddr == 0) {
      // if we didn't get the monitor command response, we will try to read the system table pointer from the core
      // which may work, if we already have loaded the core symbols. If not, we will fail gracefully. This would be the
      // case for the QEMU debugger, where we don't have the monitor command available, but we do have the
      // system table pointer symbols loaded.
      SystemPtrAddr = GetExpression ("patina_dxe_core::config_tables::debug_image_info_table::DBG_SYSTEM_TABLE_POINTER_ADDRESS");
      if (!ReadPointer (SystemPtrAddr, &SystemPtrAddr)) {
        dprintf ("Failed to read memory at %llx to get system table from ptr\n", SystemPtrAddr);
        return ERROR_NOT_FOUND;
      }
    }
  }

  if (SystemPtrAddr == NULL) {
    // TODO: Add a flag to indicate whether we should scan memory for the system table pointer and then make the
    // scanning better, maybe binary search (though has issues). For now, C DXE has parity with before, Rust has
    // two cases, we don't have the monitor command yet, but that is only true at the initial breakpoint (gets set up
    // very soon after that, before other modules are loaded, so we have already succeeded) or we are in an older Rust
    // core that doesn't support the monitor command
    return ERROR_NOT_FOUND;

    /*
    // Locate the system table pointer, which is allocated on a 4MB boundary near the top of memory
    // with signature EFI_SYSTEM_TABLE_SIGNATURE       SIGNATURE_64 ('I','B','I',' ','S','Y','S','T')
    // and the EFI_SYSTEM_TABLE structure.
    SystemPtrAddr = 0x80000000; // Start at the top of memory, well, as far as we want to go. This is pretty lazy, but it takes a long time to search the entire memory space.
    while (SystemPtrAddr >= 0x400000) { // Stop at 4MB boundary
      if (!ReadPointer(SystemPtrAddr, &Signature)) {
        SystemPtrAddr -= 0x400000; // Move to the next 4MB boundary
        continue;
      }

      if (Signature == SystemTableSignature) {
        dprintf("Found EFI_SYSTEM_TABLE_SIGNATURE at %llx\n", SystemPtrAddr);
        break;
      }

      SystemPtrAddr -= 0x400000; // Move to the next 4MB boundary
    }

    if (SystemPtrAddr < 0x400000) {
      dprintf("Failed to locate EFI_SYSTEM_TABLE_SIGNATURE!\n");
      return ERROR_NOT_FOUND;
    }
    */
  } else {
    // Check the signature at the system table pointer address
    if (!ReadPointer (SystemPtrAddr, &Signature)) {
      dprintf ("Failed to read memory at %llx to get system table signature\n", SystemPtrAddr);
      return ERROR_NOT_FOUND;
    }

    if (Signature != SystemTableSignature) {
      dprintf ("Couldn't find EFI_SYSTEM_TABLE_SIGNATURE %llx at %llx, found %llx instead\n", SystemTableSignature, SystemPtrAddr, Signature);
      return ERROR_NOT_FOUND;
    }
  }

  // move past the signature to get the EFI_SYSTEM_TABLE structure
  SystemPtrAddr += sizeof (UINT64);

  if (!ReadPointer (SystemPtrAddr, &SystemTableAddr)) {
    dprintf ("Failed to find the system table!\n");
    return ERROR_NOT_FOUND;
  }

  //
  // Load all the other modules.
  //

  Result = loadmodules (SystemTableAddr);

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

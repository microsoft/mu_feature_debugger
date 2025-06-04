/*++

  Copyright (c) Microsoft Corporation.

  SPDX-License-Identifier: BSD-2-Clause-Patent

Module Name:
  uefispec.h

Abstract:
  This file contains definitions and structures used in UEFI debugging that are defined in the UEFI spec. Note,
  the extension does not use all fields of these structures, so to avoid pulling in unnecessary types, some types
  are reduced to their base type, e.g. CUSTOM_STRUCT_TYPE * is converted to VOID * to not have to pull in the
  CUSTOM_STRUCT_TYPE definition and all of its dependent types.

--*/

#include <windows.h>

#ifndef UEFISPEC_H_
#define UEFISPEC_H_

typedef enum {
  EfiReservedMemoryType,
  EfiLoaderCode,
  EfiLoaderData,
  EfiBootServicesCode,
  EfiBootServicesData,
  EfiRuntimeServicesCode,
  EfiRuntimeServicesData,
  EfiConventionalMemory,
  EfiUnusableMemory,
  EfiACPIReclaimMemory,
  EfiACPIMemoryNVS,
  EfiMemoryMappedIO,
  EfiMemoryMappedIOPortSpace,
  EfiPalCode,
  EfiPersistentMemory,
  EfiMaxMemoryType,
  MEMORY_TYPE_OEM_RESERVED_MIN = 0x70000000,
  MEMORY_TYPE_OEM_RESERVED_MAX = 0x7FFFFFFF,
  MEMORY_TYPE_OS_RESERVED_MIN  = 0x80000000,
  MEMORY_TYPE_OS_RESERVED_MAX  = 0xFFFFFFFF
} EFI_MEMORY_TYPE;

///
/// Contains a set of GUID/pointer pairs comprised of the ConfigurationTable field in the
/// EFI System Table.
///
typedef struct {
  ///
  /// The 128-bit GUID value that uniquely identifies the system configuration table.
  ///
  GUID    VendorGuid;
  ///
  /// A pointer to the table associated with VendorGuid.
  ///
  VOID    *VendorTable;
} EFI_CONFIGURATION_TABLE;

///
/// Data structure that precedes all of the standard EFI table types.
///
typedef struct {
  ///
  /// A 64-bit signature that identifies the type of table that follows.
  /// Unique signatures have been generated for the EFI System Table,
  /// the EFI Boot Services Table, and the EFI Runtime Services Table.
  ///
  UINT64    Signature;
  ///
  /// The revision of the EFI Specification to which this table
  /// conforms. The upper 16 bits of this field contain the major
  /// revision value, and the lower 16 bits contain the minor revision
  /// value. The minor revision values are limited to the range of 00..99.
  ///
  UINT32    Revision;
  ///
  /// The size, in bytes, of the entire table including the EFI_TABLE_HEADER.
  ///
  UINT32    HeaderSize;
  ///
  /// The 32-bit CRC for the entire table. This value is computed by
  /// setting this field to 0, and computing the 32-bit CRC for HeaderSize bytes.
  ///
  UINT32    CRC32;
  ///
  /// Reserved field that must be set to 0.
  ///
  UINT32    Reserved;
} EFI_TABLE_HEADER;

///
/// EFI System Table
///
typedef struct {
  ///
  /// The table header for the EFI System Table.
  ///
  EFI_TABLE_HEADER           Hdr;
  ///
  /// A pointer to a null terminated string that identifies the vendor
  /// that produces the system firmware for the platform.
  ///
  VOID                       *FirmwareVendor;
  ///
  /// A firmware vendor specific value that identifies the revision
  /// of the system firmware for the platform.
  ///
  UINT32                     FirmwareRevision;
  ///
  /// The handle for the active console input device. This handle must support
  /// EFI_SIMPLE_TEXT_INPUT_PROTOCOL and EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL. If
  /// there is no active console, these protocols must still be present.
  ///
  VOID                       *ConsoleInHandle;
  ///
  /// A pointer to the EFI_SIMPLE_TEXT_INPUT_PROTOCOL interface that is
  /// associated with ConsoleInHandle.
  ///
  VOID                       *ConIn;
  ///
  /// The handle for the active console output device. This handle must support the
  /// EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL. If there is no active console, these protocols
  /// must still be present.
  ///
  VOID                       *ConsoleOutHandle;
  ///
  /// A pointer to the EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL interface
  /// that is associated with ConsoleOutHandle.
  ///
  VOID                       *ConOut;
  ///
  /// The handle for the active standard error console device.
  /// This handle must support the EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL. If there
  /// is no active console, this protocol must still be present.
  ///
  VOID                       *StandardErrorHandle;
  ///
  /// A pointer to the EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL interface
  /// that is associated with StandardErrorHandle.
  ///
  VOID                       *StdErr;
  ///
  /// A pointer to the EFI Runtime Services Table.
  ///
  VOID                       *RuntimeServices;
  ///
  /// A pointer to the EFI Boot Services Table.
  ///
  VOID                       *BootServices;
  ///
  /// The number of system configuration tables in the buffer ConfigurationTable.
  ///
  UINT64                     NumberOfTableEntries;
  ///
  /// A pointer to the system configuration tables.
  /// The number of entries in the table is NumberOfTableEntries.
  ///
  EFI_CONFIGURATION_TABLE    *ConfigurationTable;
} EFI_SYSTEM_TABLE;

typedef
UINT64
(*EFI_IMAGE_UNLOAD)(
  IN  VOID  *ImageHandle
  );

///
/// Can be used on any image handle to obtain information about the loaded image.
///
typedef struct {
  UINT32              Revision;        ///< Defines the revision of the EFI_LOADED_IMAGE_PROTOCOL structure.
                                       ///< All future revisions will be backward compatible to the current revision.
  VOID                *ParentHandle;   ///< Parent image's image handle. NULL if the image is loaded directly from
  ///< the firmware's boot manager.
  EFI_SYSTEM_TABLE    *SystemTable;         ///< the image's EFI system table pointer.

  //
  // Source location of image
  //
  VOID                *DeviceHandle; ///< The device handle that the EFI Image was loaded from.
  VOID                *FilePath;     ///< A pointer to the file path portion specific to DeviceHandle
  ///< that the EFI Image was loaded from.
  VOID                *Reserved;            ///< Reserved. DO NOT USE.

  //
  // Images load options
  //
  UINT32              LoadOptionsSize;         ///< The size in bytes of LoadOptions.
  VOID                *LoadOptions;            ///< A pointer to the image's binary load options.

  //
  // Location of where image was loaded
  //
  VOID                *ImageBase;            ///< The base address at which the image was loaded.
  UINT64              ImageSize;             ///< The size in bytes of the loaded image.
  EFI_MEMORY_TYPE     ImageCodeType;         ///< The memory type that the code sections were loaded as.
  EFI_MEMORY_TYPE     ImageDataType;         ///< The memory type that the data sections were loaded as.
  EFI_IMAGE_UNLOAD    Unload;
} EFI_LOADED_IMAGE_PROTOCOL;

typedef struct {
  ///
  /// Indicates the type of image info structure. For PE32 EFI images,
  /// this is set to EFI_DEBUG_IMAGE_INFO_TYPE_NORMAL.
  ///
  UINT32                       ImageInfoType;
  ///
  /// A pointer to an instance of the loaded image protocol for the associated image.
  ///
  EFI_LOADED_IMAGE_PROTOCOL    *LoadedImageProtocolInstance;
  ///
  /// Indicates the image handle of the associated image.
  ///
  VOID                         *ImageHandle;
} EFI_DEBUG_IMAGE_INFO_NORMAL;

typedef union {
  UINT32                         *ImageInfoType;
  EFI_DEBUG_IMAGE_INFO_NORMAL    *NormalImage;
} EFI_DEBUG_IMAGE_INFO;

typedef struct {
  ///
  /// UpdateStatus is used by the system to indicate the state of the debug image info table.
  ///
  volatile UINT32         UpdateStatus;
  ///
  /// The number of EFI_DEBUG_IMAGE_INFO elements in the array pointed to by EfiDebugImageInfoTable.
  ///
  UINT32                  TableSize;
  ///
  /// A pointer to the first element of an array of EFI_DEBUG_IMAGE_INFO structures.
  ///
  EFI_DEBUG_IMAGE_INFO    *EfiDebugImageInfoTable;
} EFI_DEBUG_IMAGE_INFO_TABLE_HEADER;

#define EFI_DEBUG_IMAGE_INFO_TABLE_GUID \
  { \
    0x49152e77, 0x1ada, 0x4764, {0xb7, 0xa2, 0x7a, 0xfe, 0xfe, 0xd9, 0x5e, 0x8b } \
  }

#endif // UEFISPEC_H_

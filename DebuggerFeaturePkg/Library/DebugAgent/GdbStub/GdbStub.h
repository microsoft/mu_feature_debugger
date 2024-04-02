/** @file
  Header definition for GDB stub.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef GDB_STUB_H_
#define GDB_STUB_H_

//
// GDB Error Codes. These are specific to this GDB implementation as these numbers
// are not well defined.
//

#define GDB_ERROR_NONE               0x00
#define GDB_ERROR_UNSUPPORTED        0x01
#define GDB_ERROR_INTERNAL           0x02
#define GDB_ERROR_UNKOWN_CMD         0x03
#define GDB_ERROR_BAD_REQUEST        0x04
#define GDB_ERROR_BAD_REG_INDEX      0x05
#define GDB_ERROR_BAD_MEM_ADDRESS    0x06
#define GDB_ERROR_RESPONSE_TOO_LONG  0x07

//
// Architecture specific logic structures.
//

typedef struct _GDB_REGISTER_OFFSET_DATA {
  UINTN          Offset;
  UINTN          Size;
  CONST CHAR8    *Name;
  CONST CHAR8    *Type;
} GDB_REGISTER_OFFSET_DATA;

// Indicates the register is not available in system context and reads will result
// in all 0's and writes will be ignored.
#define REG_NOT_PRESENT  (0xFFFFFFFF)

extern GDB_REGISTER_OFFSET_DATA  gRegisterOffsets[];
extern UINTN                     gRegisterCount;

typedef struct _GDB_TARGET_INFO {
  CONST CHAR8    *TargetArch;
  CONST CHAR8    *RegistersFeature;
} GDB_TARGET_INFO;

extern CONST GDB_TARGET_INFO  GdbTargetInfo;

//
// Routine implemented per architecture.
//

VOID
GdbReadMsr (
  IN CHAR8   *Cmd,
  OUT CHAR8  *Response,
  IN UINTN   BufferLength
  );

VOID
GdbDumpSystemRegisters (
  IN CHAR8   *Cmd,
  OUT CHAR8  *Response,
  IN UINTN   BufferLength
  );

#endif

/** @file
  X64 implementations of GDB Stub.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Protocol/Cpu.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>

#include <Library/UefiLib.h>
#include "DebugAgent.h"
#include "GdbStub.h"

GDB_REGISTER_OFFSET_DATA  gRegisterOffsets[] = {
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X0),   8, "x0",   "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X1),   8, "x1",   "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X2),   8, "x2",   "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X3),   8, "x3",   "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X4),   8, "x4",   "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X5),   8, "x5",   "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X6),   8, "x6",   "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X7),   8, "x7",   "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X8),   8, "x8",   "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X9),   8, "x9",   "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X10),  8, "x10",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X11),  8, "x11",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X12),  8, "x12",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X13),  8, "x13",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X14),  8, "x14",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X15),  8, "x15",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X16),  8, "x16",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X17),  8, "x17",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X18),  8, "x18",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X19),  8, "x19",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X20),  8, "x20",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X21),  8, "x21",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X22),  8, "x22",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X23),  8, "x23",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X24),  8, "x24",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X25),  8, "x25",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X26),  8, "x26",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X27),  8, "x27",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, X28),  8, "x28",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, FP),   8, "x29",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, LR),   8, "x30",  "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, SP),   8, "sp",   "data_ptr" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, ELR),  8, "pc",   "code_ptr" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, FPSR), 8, "fpcr", "int64"    },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_AARCH64, SPSR), 4, "cpsr", "int32"    },
};

UINTN  gRegisterCount = (sizeof (gRegisterOffsets) / sizeof (GDB_REGISTER_OFFSET_DATA));

CONST GDB_TARGET_INFO  GdbTargetInfo = {
  "aarch64",
  "org.gnu.gdb.aarch64.core"
};

/**
  Read MSR into a string response.

  @param[in]  Cmd           The command string.
  @param[out] Response      The buffer to write the response to.
  @param[in]  BufferLength  The length of the provided buffer.

**/
VOID
GdbReadMsr (
  IN CHAR8   *Cmd,
  OUT CHAR8  *Response,
  IN UINTN   BufferLength
  )
{
  AsciiSPrint (Response, BufferLength, "Not supported for AARCH64.\n\r");
}

/**
  Dumps system registers.

  @param[in]  Cmd           The command string.
  @param[out] Response      The buffer to write the response to.
  @param[in]  BufferLength  The length of the provided buffer.

**/
VOID
GdbDumpSystemRegisters (
  IN CHAR8   *Cmd,
  OUT CHAR8  *Response,
  IN UINTN   BufferLength
  )
{
  AsciiSPrint (Response, BufferLength, "TODO.\n\r");
}

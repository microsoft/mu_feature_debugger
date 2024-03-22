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

#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include "DebugAgent.h"
#include "GdbStub.h"

GDB_REGISTER_OFFSET_DATA  gRegisterOffsets[] = {
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Rax),    8, "rax",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Rbx),    8, "rbx",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Rcx),    8, "rcx",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Rdx),    8, "rdx",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Rsi),    8, "rsi",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Rdi),    8, "rdi",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Rbp),    8, "rbp",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Rsp),    8, "rsp",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, R8),     8, "r8",     "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, R9),     8, "r9",     "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, R10),    8, "r10",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, R11),    8, "r11",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, R12),    8, "r12",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, R13),    8, "r13",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, R14),    8, "r14",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, R15),    8, "r15",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Rip),    8, "rip",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Rflags), 8, "eflags", "int64" }, // GDB require eflags, tuncate.
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Cs),     4, "cs",     "int32" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Ss),     4, "ss",     "int32" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Ds),     4, "ds",     "int32" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Es),     4, "es",     "int32" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Fs),     4, "fs",     "int32" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Gs),     4, "gs",     "int32" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Cr0),    8, "cr0",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Cr2),    8, "cr2",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Cr3),    8, "cr3",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Cr4),    8, "cr4",    "int64" },
  { OFFSET_OF (EFI_SYSTEM_CONTEXT_X64, Cr8),    8, "cr8",    "int64" },
  { REG_NOT_PRESENT,                            4, "fctrl",  "int" },
  { REG_NOT_PRESENT,                            4, "fstat",  "int" },
  { REG_NOT_PRESENT,                            4, "ftag",   "int" },
  { REG_NOT_PRESENT,                            4, "fiseg",  "int" },
  { REG_NOT_PRESENT,                            4, "fioff",  "int" },
  { REG_NOT_PRESENT,                            4, "foseg",  "int" },
  { REG_NOT_PRESENT,                            4, "fooff",  "int" },
  { REG_NOT_PRESENT,                           10, "fop",   "i387_ext" },
  { REG_NOT_PRESENT,                           10, "st0",   "i387_ext" },
  { REG_NOT_PRESENT,                           10, "st1",   "i387_ext" },
  { REG_NOT_PRESENT,                           10, "st2",   "i387_ext" },
  { REG_NOT_PRESENT,                           10, "st3",   "i387_ext" },
  { REG_NOT_PRESENT,                           10, "st4",   "i387_ext" },
  { REG_NOT_PRESENT,                           10, "st5",   "i387_ext" },
  { REG_NOT_PRESENT,                           10, "st6",   "i387_ext" },
  { REG_NOT_PRESENT,                           10, "st7",   "i387_ext" }
};

UINTN  gRegisterCount = (sizeof (gRegisterOffsets) / sizeof (GDB_REGISTER_OFFSET_DATA));

CONST GDB_TARGET_INFO  GdbTargetInfo = {
  "i386:x86-64",
  "org.gnu.gdb.i386.core"
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
  UINT64  MsrNumber;
  UINT64  MsrValue;

  MsrNumber = AsciiStrHexToUintn (Cmd);
  MsrValue  = AsmReadMsr64 ((UINT32)MsrNumber);
  AsciiSPrint (Response, BufferLength, "MSR %08x = %016x\n\r", MsrNumber, MsrValue);
}

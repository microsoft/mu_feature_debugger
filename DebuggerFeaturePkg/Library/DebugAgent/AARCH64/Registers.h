/** @file
  Prototypes for assembly routines for accessing system registers.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef DEBUG_REGISTERS_H__
#define DEBUG_REGISTERS_H__

UINT64
DebugReadMdscrEl1 (
  VOID
  );

VOID
DebugWriteMdscrEl1 (
  IN UINT64  Value
  );

UINT64
DebugReadDaif (
  VOID
  );

VOID
DebugWriteDaif (
  IN UINT64  Value
  );

UINT64
DebugReadOslsrEl1 (
  VOID
  );

VOID
DebugWriteOslarEl1 (
  IN UINT64  Value
  );

UINT64
DebugGetTCR (
  VOID
  );

UINT64
DebugGetTTBR0BaseAddress (
  VOID
  );

UINT64
DebugReadDbgWvr0El1 (
  VOID
  );

VOID
DebugWriteDbgWvr0El1 (
  IN UINT64  Value
  );

UINT64
DebugReadDbgWcr0El1 (
  VOID
  );

VOID
DebugWriteDbgWcr0El1 (
  IN UINT64  Value
  );

UINT64
DebugReadDbgWvr1El1 (
  VOID
  );

VOID
DebugWriteDbgWvr1El1 (
  IN UINT64  Value
  );

UINT64
DebugReadDbgWcr1El1 (
  VOID
  );

VOID
DebugWriteDbgWcr1El1 (
  IN UINT64  Value
  );

UINT64
DebugReadDbgWvr2El1 (
  VOID
  );

VOID
DebugWriteDbgWvr2El1 (
  IN UINT64  Value
  );

UINT64
DebugReadDbgWcr2El1 (
  VOID
  );

VOID
DebugWriteDbgWcr2El1 (
  IN UINT64  Value
  );

UINT64
DebugReadDbgWvr3El1 (
  VOID
  );

VOID
DebugWriteDbgWvr3El1 (
  IN UINT64  Value
  );

UINT64
DebugReadDbgWcr3El1 (
  VOID
  );

VOID
DebugWriteDbgWcr3El1 (
  IN UINT64  Value
  );

#endif

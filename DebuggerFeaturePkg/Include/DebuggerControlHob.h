/** @file
  Definition for the Debugger Configuration Hob

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef DEBUGGER_CONTROL_HOB_H_
#define DEBUGGER_CONTROL_HOB_H_

typedef struct _DEBUGGER_CONTROL_HOB {
  union {
    struct {
      UINT32    InitialBreakpoint : 1;
      UINT32    DxeDebugEnabled   : 1;
      UINT32    MmDebugEnabled    : 1;
      UINT32    Unused            : 29;
    } Flags;

    UINT32    AsUint32;
  } Control;

  // The frequency of the TSC in milliseconds. Only used for X64.
  UINT64    PerformanceCounterFreq;

  UINT64    InitialBreakpointTimeout;
} DEBUGGER_CONTROL_HOB;

#endif

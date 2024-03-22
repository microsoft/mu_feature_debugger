/**@file WatchdogTimerLib.h

  This module contains code to interact with the watchdog timer.

  Copyright (c) Microsoft Corporation.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/TransportLogControlLib.h>

/**
  Suspends any logging occurring on the debug transport.

**/
VOID
TransportLogSuspend (
  VOID
  )
{
}

/**
  Resumes logging on the debug transport.

**/
VOID
TransportLogResume (
  VOID
  )
{
}

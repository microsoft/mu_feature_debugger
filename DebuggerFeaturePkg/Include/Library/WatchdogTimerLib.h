/**@file WatchdogTimerLib.h

  This module contains code to interact with the watchdog timer.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef WATCHDOG_TIMER_LIB_H_
#define WATCHDOG_TIMER_LIB_H_

/**
  Suspends any watchdog timers that may be active on the platform.

  @retval   TRUE    Watchdog timer was previously running.
  @retval   FALSE   No watchdog timer was running.
**/
BOOLEAN
WatchdogSuspend (
  );

/**
  Resumes the execution of the watchdog timer.

  @param[in]    PreviouslyRunning   Indicates a watchdog timer was previously running
                                    and was suspended.

**/
VOID
WatchdogResume (
  IN BOOLEAN  PreviouslyRunning
  );

#endif

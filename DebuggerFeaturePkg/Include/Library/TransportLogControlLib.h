/**@file WatchdogTimerLib.h

  This module contains code to control logging on the transport library.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef TRANSPORT_LOG_CONTROL_LIB_H_
#define TRANSPORT_LOG_CONTROL_LIB_H_

/**
  Suspends any logging occurring on the debug transport.

**/
VOID
TransportLogSuspend (
  VOID
  );

/**
  Resumes logging on the debug transport.

**/
VOID
TransportLogResume (
  VOID
  );

#endif

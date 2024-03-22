/**

 Definitions for the debug transport library. Used by a debug agent to communicate
 with the debugger.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef DEBUG_TRANSPORT_LIB_H_
#define DEBUG_TRANSPORT_LIB_H_

/**
  Initializes the debug transport if needed.

  @retval   EFI_SUCCESS   The debug transport was successfully initialized.
  @retval   Other         The debug transport initialization failed.
**/
EFI_STATUS
EFIAPI
DebugTransportInitialize (
  VOID
  );

/**
  Reads data from the debug transport.

  @param[out]   Buffer          The buffer to read the data to.
  @param[out]   NumberOfBytes   The number of bytes to read from the transport.
  @param[out]   Timeout         The timeout in Milliseconds. May not be supported
                                by all transports.

  @retval       The number of bytes read from the transport.
**/
UINTN
EFIAPI
DebugTransportRead (
  OUT UINT8  *Buffer,
  IN UINTN   NumberOfBytes,
  IN UINTN   Timeout
  );

/**
  Writes data to the debug transport.

  @param[out]   Buffer          The buffer of the data to be written.
  @param[out]   NumberOfBytes   The number of bytes to write to the transport.

  @retval       The number of bytes written to the transport.
**/
UINTN
EFIAPI
DebugTransportWrite (
  IN UINT8  *Buffer,
  IN UINTN  NumberOfBytes
  );

/**
  Checks if there is pending data to read.

  @retval   TRUE    Data is pending read from the transport
  @retval   FALSE   There is no data is pending read from the transport
**/
BOOLEAN
EFIAPI
DebugTransportPoll (
  VOID
  );

#endif

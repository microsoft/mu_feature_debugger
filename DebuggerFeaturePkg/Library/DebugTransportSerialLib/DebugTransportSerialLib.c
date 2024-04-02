/** @file
  Implementation of the DebugTransportLib using the serial port library.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi/UefiBaseType.h>

#include <Library/PcdLib.h>
#include <Library/SerialPortLib.h>

extern BOOLEAN  gDbgDisableLogHwPort;

/**
  Initializes the debug transport if needed.

  @retval   EFI_SUCCESS   The debug transport was successfully initialized.
  @retval   Other         The debug transport initialization failed.
**/
EFI_STATUS
EFIAPI
DebugTransportInitialize (
  VOID
  )
{
  return SerialPortInitialize ();
}

/**
  Reads data from the debug transport.

  @param[out]   Buffer          The buffer to read the data to.
  @param[out]   NumberOfBytes   The number of bytes to read from the transport.
  @param[out]   Timeout         UNUSED

  @retval       The number of bytes read from the transport.
**/
UINTN
EFIAPI
DebugTransportRead (
  OUT UINT8  *Buffer,
  IN UINTN   NumberOfBytes,
  IN UINTN   Timeout
  )
{
  return SerialPortRead (Buffer, NumberOfBytes);
}

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
  )
{
  return SerialPortWrite (Buffer, NumberOfBytes);
}

/**
  Checks if there is pending data to read.

  @retval   TRUE    Data is pending read from the transport
  @retval   FALSE   There is no data is pending read from the transport
**/
BOOLEAN
EFIAPI
DebugTransportPoll (
  VOID
  )
{
  return SerialPortPoll ();
}

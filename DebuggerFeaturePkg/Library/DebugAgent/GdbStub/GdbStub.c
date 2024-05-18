/** @file

  The implementation of the GDB stub for the UEFI debugger. This implementation
  followings the API detailed here https://sourceware.org/gdb/current/onlinedocs/gdb.html/Remote-Protocol.html.

  Copyright (c) Microsoft Corporation.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Protocol/Cpu.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DebugTransportLib.h>
#include <Library/TransportLogControlLib.h>

#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include <Library/TimerLib.h>
#include "DebugAgent.h"
#include "GdbStub.h"

//
// Constant definitions.
//

#define MAX_REQUEST_SIZE   2048
#define MAX_RESPONSE_SIZE  0x1000
#define SCRATCH_SIZE       1024

// Used for quick translation of numbers into HEX.
STATIC CONST UINT8  HexChars[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

STATIC CONST CHAR8  *EXCEPTION_TYPE_STRINGS[] = {
  "ExceptionDebugStep",
  "ExceptionBreakpoint",
  "ExceptionGenericFault",
  "ExceptionInvalidOp",
  "ExceptionAlignment",
  "ExceptionAccessViolation"
};

STATIC CONST CHAR8  *BREAK_REASON_STRINGS[] = {
  "N/A",
  "Initial Breakpoint",
  "Module Load",
  "Debugger Break"
};

//
// Current state
//

EFI_SYSTEM_CONTEXT  *gSystemContext   = NULL;
EXCEPTION_INFO      *gExceptionInfo   = NULL;
BOOLEAN             gRunning          = TRUE;
BOOLEAN             mRebootOnContinue = FALSE;

//
// Buffers for GDB packets. Memory allocation are not available at first, so these
// must be static.
//

// Used to store the incoming request packet.
STATIC CHAR8  mRequest[MAX_REQUEST_SIZE];

// Used to store the full response packet to be sent. Only to be used by SendGdbResponse.
// Extra space for $ <PACKET> #NN and
STATIC CHAR8  mResponseFull[1 + MAX_RESPONSE_SIZE + 3 + 2];

// Used by any function to start writing the response packet.
STATIC CHAR8  *mResponse = &mResponseFull[1];

// Used for storing data temporarily.
STATIC CHAR8  mScratch[SCRATCH_SIZE];

// Tracks if the previous response was acknowledged by the debugger.
STATIC BOOLEAN  mResponseAcknowledged = FALSE;

// Used to set a timeout for the next breakpoint.
STATIC volatile UINT64  mNextBreakpointTimeout = 0;

// Tracks if successful communication has occurred with a debugger.
STATIC BOOLEAN  mConnectionOccurred;

/**
  Read a byte from the debug transport.

  @param[in] Byte           Pointer where to store the byte that was read.
  @param[in] Timeout        How many milliseconds to try to read.

**/
STATIC
BOOLEAN
DebugReadByte (
  UINT8   *Byte,
  UINT32  Timeout
  )
{
  UINT64  EndTime;

  EndTime = DebugGetTimeMs () + Timeout;
  do {
    if (DebugTransportPoll ()) {
      DebugTransportRead (Byte, 1, Timeout);
      return TRUE;
    }
  } while (DebugGetTimeMs () < EndTime);

  return FALSE;
}

/**
  Sends a GDB acknowledge packet.

  @param[in] Positive  Indicates that a positive acknowledgement should be sent.

**/
STATIC
VOID
SendGdbAck (
  BOOLEAN  Positive
  )
{
  if (Positive) {
    DebugTransportWrite ((UINT8 *)"+", 1);
  } else {
    DebugTransportWrite ((UINT8 *)"-", 1);
  }
}

/**
  Sends a GDB notification packet with log information.

  @param[in] FormatString  Format string of the log message.
  @param[in] VA_LIST       Additional arguments for the formatting.

**/
/*
STATIC
VOID
GdbNotifyLog (
  IN  CONST CHAR8  *FormatString,
  ...
  )
{
  VA_LIST  Marker;
  UINTN    Length;
  UINT8    Checksum;
  CHAR8    String[64];

  Length = sizeof ("%log:") - 1;
  CopyMem (String, "%log:", Length);

  VA_START (Marker, FormatString);
  Length += AsciiVSPrint (&String[Length], sizeof (String) - Length - 4, FormatString, Marker);
  VA_END (Marker);

  Checksum           = CalculateSum8 ((UINT8 *)&String[1], Length - 1);
  String[Length]     = '#';
  String[Length + 1] = HexChars[Checksum >> 4];
  String[Length + 2] = HexChars[Checksum & 0xF];
  String[Length + 3] = 0;
  Length            += 3;

  DebugTransportWrite ((UINT8 *)&String[0], Length);
}
*/

/**
  Sends a checksummed GDB packet response.

  @param[in] Response   The NULL terminated string of the response. The NULL,
                        will resend last packet.
**/
STATIC
VOID
SendGdbResponse (
  CHAR8  *Response
  )
{
  UINT8         Checksum;
  STATIC UINTN  ResponseLength = 0;

  if (Response == NULL) {
    // This is requesting a resend.
    ASSERT (!mResponseAcknowledged);
    ASSERT (ResponseLength >= 4);
  } else {
    ResponseLength = AsciiStrLen (Response);
    ASSERT (ResponseLength <= MAX_RESPONSE_SIZE);
    Checksum = CalculateSum8 ((UINT8 *)Response, ResponseLength);

    mResponseFull[0] = '$';

    // Only copy the data if the caller is not using the nested mResponse pointer.
    if (&mResponseFull[1] != &Response[0]) {
      CopyMem (&mResponseFull[1], &Response[0], ResponseLength);
    }

    mResponseFull[ResponseLength + 1] = '#';
    mResponseFull[ResponseLength + 2] = HexChars[Checksum >> 4];
    mResponseFull[ResponseLength + 3] = HexChars[Checksum & 0xF];
    mResponseFull[ResponseLength + 4] = 0;
    ResponseLength                   += 4;
  }

  mResponseAcknowledged = FALSE;
  // DEBUG ((DEBUG_INFO, "Response '%a' \n", ResponseFull));
  DebugTransportWrite ((UINT8 *)&mResponseFull[0], ResponseLength);
}

/**
  Converts a 2-byte ASCII HEX string to the byte value.

  @param[in]  Chars  Two HEX chars to be converted into a byte value.

  @retval   The byte value of the hex characters.
**/
UINT8
HexToByte (
  CHAR8  Chars[2]
  )
{
  UINT8  Result;
  UINTN  Index;
  CHAR8  Char;

  Result = 0;
  for (Index = 0; Index < 2; Index++) {
    Char   = Chars[Index];
    Result = Result << 4;
    if (('0' <= Char) && (Char <= '9')) {
      Result |= (Char - '0');
    } else {
      Char = AsciiCharToUpper (Char);
      ASSERT (('A' <= Char) && (Char <= 'F'));
      Result |= (10 + Char - 'A');
    }
  }

  return Result;
}

/**
  Converts a string to hex values string.

  @param[in]  Response      The null terminated response string in ASCII.
  @param[in]  Output        The buffer to print the hex string.
  @param[in]  BufferLength  The length of the Output buffer.

**/
VOID
ConvertResponseToHex (
  IN CHAR8   *Response,
  OUT CHAR8  *Output,
  IN UINTN   BufferLength
  )
{
  UINTN  Index;
  UINT8  Byte;

  //
  // Write each ASCII char into two HEX characters, making sure to leave room
  // for the null character at the end of the string.
  //

  Index = 0;
  while ((Response[Index] != 0) && (((Index * 2) + 1) < (BufferLength - 1))) {
    Byte                    = Response[Index];
    Output[Index * 2]       = HexChars[(Byte & 0xF0) >> 4];
    Output[(Index * 2) + 1] = HexChars[Byte & 0xF];
    Index++;
  }

  Output[Index * 2] = 0;
}

/**
  Sends a GDB error response packet

  @param[in]  ErrorCode  The error code to send.
**/
VOID
SendGdbError (
  UINT8  ErrorCode
  )
{
  CHAR8  ErrorString[4];

  ErrorString[0] = 'E';
  ErrorString[1] = HexChars[ErrorCode >> 4];
  ErrorString[2] = HexChars[ErrorCode & 0xF];
  ErrorString[3] = 0;
  SendGdbResponse (&ErrorString[0]);
}

/**
  Sends the GDB stop reason reply packet.

**/
STATIC
VOID
SendStopReply (
  )
{
  switch (gExceptionInfo->ExceptionType) {
    // TODO add more specific stop reasons for GDB support.
    default:
      SendGdbResponse ("T05thread:01;");
  }
}

/**
  Processes a multi-letter named GDB packet.

  @param[in]  Command         The V  command.
  @param[in]  CommandLength   The length of the command.

**/
STATIC
VOID
ProcessVCommand (
  CHAR8   *Command,
  UINT32  CommandLength
  )
{
  UINT8   OriginalDelimiter;
  UINT32  DelimiterIndex;

  // Replace the ; or ? with a terminator;
  for (DelimiterIndex = 0; DelimiterIndex < CommandLength; DelimiterIndex++) {
    if ((Command[DelimiterIndex] == ';') || (Command[DelimiterIndex] == '?')) {
      break;
    }
  }

  OriginalDelimiter = Command[DelimiterIndex];
  ASSERT ((OriginalDelimiter == ';') || (OriginalDelimiter == '?') || (OriginalDelimiter == 0));
  Command[DelimiterIndex] = 0;

  if (AsciiStrnCmp (Command, "Cont", DelimiterIndex) == 0) {
    if (OriginalDelimiter == ';') {
      if ((Command[DelimiterIndex + 1] == 'c')) {
        // continue
        gRunning = TRUE;
        return;
      } else if ((Command[DelimiterIndex + 1] == 's')) {
        // step
        AddSingleStep (gSystemContext);
        gRunning = TRUE;
        return;
      }
    } else if (OriginalDelimiter == '?') {
      SendGdbResponse ("vCont;c;C;s;S");
      return;
    }
  }

  // Spec dictates to respond with an empty string for an unknown V command.
  SendGdbResponse ("");
}

/**
  Parses and a memory command and sends response.

  @param[in]  Write           Indicates if this is a write command.
  @param[in]  Command         The memory command.
  @param[in]  CommandLength   The length of the memory command.

**/
STATIC
VOID
ProcessMemoryCommand (
  BOOLEAN  Write,
  CHAR8    *Command,
  UINT32   CommandLength
  )
{
  CHAR8       *AddressString;
  CHAR8       *LengthString;
  CHAR8       *ValueString;
  UINT64      Address;
  UINT64      Length;
  UINT64      RangeLength;
  EFI_STATUS  Status;
  UINTN       RangeIndex;
  UINTN       RespIndex;
  UINT8       Byte;

  RespIndex     = 0;
  AddressString = &Command[0];
  LengthString  = ScanMem8 (&Command[0], CommandLength, ',');
  if (LengthString == NULL) {
    SendGdbError (GDB_ERROR_BAD_REQUEST);
    return;
  }

  *LengthString = 0;
  LengthString += 1;

  if (Write) {
    ValueString = ScanMem8 (&Command[0], CommandLength, ':');
    if (ValueString == NULL) {
      SendGdbError (GDB_ERROR_BAD_REQUEST);
      return;
    }

    *ValueString = 0;
    ValueString += 1;
  }

  Status = AsciiStrHexToUint64S (AddressString, NULL, &Address);
  if (EFI_ERROR (Status)) {
    SendGdbError (GDB_ERROR_BAD_REQUEST);
    return;
  }

  Status = AsciiStrHexToUint64S (LengthString, NULL, &Length);
  if (EFI_ERROR (Status)) {
    SendGdbError (GDB_ERROR_BAD_REQUEST);
    return;
  }

  if (Write && (AsciiStrLen (ValueString) != Length * 2)) {
    ASSERT (FALSE);
    SendGdbError (GDB_ERROR_BAD_REQUEST);
    return;
  }

  //
  // For permission reasons, don't directly access memory. Copy into or out of a
  // buffer and operate on it from there.
  //

  while (Length > 0) {
    RangeLength = MIN (Length, sizeof (mScratch));
    if (Write) {
      for (RangeIndex = 0; RangeIndex < RangeLength; RangeIndex++) {
        mScratch[RangeIndex] = HexToByte (ValueString);
        ValueString         += 2;
      }

      if (!DbgWriteMemory (Address, &mScratch[0], RangeLength)) {
        SendGdbError (GDB_ERROR_BAD_MEM_ADDRESS);
        return;
      }
    } else {
      //
      // WORK AROUND: Windbg will try to read page 0 and the Windows Shared Data
      // page, but will loop for quite some time if those do not succeed. Just
      // return 0 so that the logic fails fast.
      //

      if (PcdGetBool (PcdEnableWindbgWorkarounds) &&
          ((Address < EFI_PAGE_SIZE) || ((Address & ~EFI_PAGE_MASK) == 0xfffff78000000000llu)) &&
          (RangeLength < EFI_PAGE_SIZE))
      {
        ZeroMem (&mScratch[0], RangeLength);
      } else if (!DbgReadMemory (Address, &mScratch[0], RangeLength)) {
        SendGdbError (GDB_ERROR_BAD_MEM_ADDRESS);
        return;
      }

      for (RangeIndex = 0; RangeIndex < Length; RangeIndex++) {
        Byte                   = mScratch[RangeIndex];
        mResponse[RespIndex++] = HexChars[(Byte & 0xF0) >> 4];
        mResponse[RespIndex++] = HexChars[Byte & 0xF];
      }
    }

    Address += RangeLength;
    Length  -= RangeLength;
  }

  if (Write) {
    SendGdbResponse ("OK");
  } else {
    mResponse[RespIndex] = 0;
    SendGdbResponse (&mResponse[0]);
  }
}

/**
  Processes a custom qRcmd,#### command. These commands are specific to the UEFI
  debugger and may be expanded with functionality as needed.

  @param[in]  CommandHex  The HEX encoded string of the command.

**/
VOID
ProcessMonitorCmd (
  CHAR8  *CommandHex
  )
{
  UINTN  Index;
  UINTN  CommandLen;
  CHAR8  Command[128];

  // The command comes in hex encoded, convert it to a byte array.
  CommandLen = AsciiStrLen (CommandHex);
  if (((CommandLen % 2) != 0) || ((CommandLen / 2) >= sizeof (Command))) {
    SendGdbError (GDB_ERROR_BAD_REQUEST);
    return;
  }

  CommandLen /= 2;
  for (Index = 0; Index < CommandLen; Index++) {
    Command[Index] = HexToByte (&CommandHex[Index * 2]);
  }

  Command[Index] = 0;

  //
  // Interpret the command. This is specific to the UEFI debugger and not
  // from the GDB specifications. Treat the first byte as the command.
  //

  mScratch[0] = 0;
  switch (Command[0]) {
    case '?': // Get UEFI debugger info.
      AsciiSPrint (
        &mScratch[0],
        SCRATCH_SIZE,
        "%a\n\r"
        "Exception Type: %a (%d)\n\r"
        "Exception Address: %llx\n\r"
        "Architecture Exception Code: 0x%llx\n\r"
        "Break Reason: %a\n\r",
        gDebuggerInfo,
        EXCEPTION_TYPE_STRINGS[gExceptionInfo->ExceptionType],
        gExceptionInfo->ExceptionType,
        gExceptionInfo->ExceptionAddress,
        gExceptionInfo->ArchExceptionCode,
        BREAK_REASON_STRINGS[DebuggerBreakpointReason]
        );

      break;

    case 'i': // Dump system registers
      GdbDumpSystemRegisters (&Command[1], &mScratch[0], SCRATCH_SIZE);
      break;

    case 'v': // Variable Read
      AsciiSPrint (&mScratch[0], SCRATCH_SIZE, "TODO\n\r");
      break;

    case 'V': // Variable Write
      AsciiSPrint (&mScratch[0], SCRATCH_SIZE, "TODO\n\r");
      break;

    case 'm': // MSR Read
      GdbReadMsr (&Command[1], &mScratch[0], SCRATCH_SIZE);
      break;

    case 'M': // MSR Write
      AsciiSPrint (&mScratch[0], SCRATCH_SIZE, "TODO\n\r");
      break;

    case 'R': // Set Reboot on Continue
      mRebootOnContinue = TRUE;
      AsciiSPrint (&mScratch[0], SCRATCH_SIZE, "Will reboot on continue.\n\r");
      break;

    case 'b': // Module Break
      if (DbgSetBreakOnModuleLoad (&Command[1])) {
        AsciiSPrint (&mScratch[0], SCRATCH_SIZE, "Will break on load for %a\n\r", &Command[1]);
      } else {
        AsciiSPrint (&mScratch[0], SCRATCH_SIZE, "FAILED to set break on load for %a\n\r", &Command[1]);
      }

      break;

    default:
      AsciiSPrint (&mScratch[0], SCRATCH_SIZE, "Unknown command '%a'\n\r", Command);
      break;
  }

  // RCmd commands return hex encoded responses, convert to HEX before sending.
  ConvertResponseToHex (&mScratch[0], mResponse, MAX_RESPONSE_SIZE);
  SendGdbResponse (mResponse);
}

/**
  Sends the target description XML.

**/
VOID
ReadTargetDescription (
  VOID
  )
{
  AsciiSPrint (
    mResponse,
    MAX_RESPONSE_SIZE,
    "l<?xml version=\"1.0\"?>"
    "<!DOCTYPE target SYSTEM \"gdb-target.dtd\">"
    "<target>"
    "<architecture>%a</architecture>"
    "<xi:include href=\"registers.xml\"/>"
    "</target>",
    GdbTargetInfo.TargetArch
    );

  SendGdbResponse (mResponse);
}

/**
  Sends the register offset information target XML file.

**/
VOID
ReadTargetRegisters (
  VOID
  )
{
  CHAR8  *End;
  UINTN  Written;
  UINTN  RegNumber;
  UINTN  Remaining;

  End       = mResponse;
  Remaining = MAX_RESPONSE_SIZE;
  Written   = AsciiSPrint (
                End,
                Remaining,
                "l<?xml version=\"1.0\"?><!DOCTYPE target SYSTEM \"gdb-target.dtd\"><feature name=\"%a\">",
                GdbTargetInfo.RegistersFeature
                );
  Remaining -= Written;
  End       += Written;

  for (RegNumber = 0; RegNumber < gRegisterCount; RegNumber++) {
    if (gRegisterOffsets[RegNumber].Name == NULL) {
      continue;
    }

    Written = AsciiSPrint (
                End,
                Remaining,
                "<reg name=\"%a\" bitsize=\"%d\" type=\"%a\" regnum=\"%d\"/>",
                gRegisterOffsets[RegNumber].Name,
                gRegisterOffsets[RegNumber].Size * 8,
                gRegisterOffsets[RegNumber].Type,
                RegNumber
                );

    Remaining -= Written;
    End       += Written;
  }

  Written    = AsciiSPrint (End, Remaining, "</feature>");
  Remaining -= Written;
  End       += Written;

  // Check if we ran out of space.
  if (Remaining == 0) {
    SendGdbError (GDB_ERROR_RESPONSE_TOO_LONG);
  } else {
    SendGdbResponse (mResponse);
  }
}

/**
  Parses a general query command.

  @param[in] Command  The general query command.

**/
VOID
ProcessQuery (
  CHAR8  *Command
  )
{
  if (AsciiStrnCmp (Command, "Supported", 9) == 0) {
    SendGdbResponse ("PacketSize=1000;qXfer:features:read+;vContSupported+");
  } else if (AsciiStrnCmp (Command, "fThreadInfo", 11) == 0) {
    // Only supports 1 thread for now.
    SendGdbResponse ("m01");
  } else if (AsciiStrnCmp (Command, "sThreadInfo", 11) == 0) {
    // Only supports 1 thread for now.
    SendGdbResponse ("l");
  } else if (AsciiStrnCmp (Command, "Xfer:features:read:target.xml", 29) == 0) {
    ReadTargetDescription ();
  } else if (AsciiStrnCmp (Command, "Xfer:features:read:registers.xml", 29) == 0) {
    ReadTargetRegisters ();
  } else if (AsciiStrnCmp (Command, "Rcmd,", 5) == 0) {
    ProcessMonitorCmd (Command + 5);
  } else if (AsciiStrnCmp (Command, "Attached", 8) == 0) {
    // Indicates we are attached to an existing process.
    SendGdbResponse ("1");
  } else {
    // Empty string indicates the query is not supported.
    SendGdbResponse ("");
  }
}

/**
  Read a register to the saved context at the specified register index.

  @param[in]  Registers  The pointer to the saved register context.
  @param[in]  RegNumber  The index of the GDB exposed register.
  @param[in]  Output     The output string of the HEX value of the register

  @retval   A pointer to the current end of the output string.
**/
CHAR8 *
ReadRegisterFromContext (
  IN  UINT8   *Registers,
  IN  UINTN   RegNumber,
  OUT  CHAR8  *Output
  )
{
  UINT8  *RegPtr;
  UINTN  i;

  if (gRegisterOffsets[RegNumber].Offset != REG_NOT_PRESENT) {
    RegPtr = Registers + gRegisterOffsets[RegNumber].Offset;

    // Parse one byte at a time.
    for (i = 0; i < gRegisterOffsets[RegNumber].Size; i++) {
      Output[i * 2]       = HexChars[RegPtr[i] >> 4];
      Output[(i * 2) + 1] = HexChars[RegPtr[i] & 0xF];
    }
  } else {
    for (i = 0; i < gRegisterOffsets[RegNumber].Size * 2; i++) {
      Output[i] = '0';
    }
  }

  return &Output[gRegisterOffsets[RegNumber].Size * 2];
}

/**
  Write a register to the saved context at the specified register index.

  @param[in]  Registers  The pointer to the saved register context.
  @param[in]  RegNumber  The index of the GDB exposed register.
  @param[in]  Input      The string representing the HEX value of the register.

  @retval   A pointer to the input HEX string + the number of characters consumed.
            NULL if an error occurred.
**/
CHAR8 *
WriteRegisterToContext (
  IN  UINT8  *Registers,
  IN  UINTN  RegNumber,
  IN  CHAR8  *Input
  )
{
  UINT8  *RegPtr;
  UINT8  Value[10];
  UINTN  Index;

  ASSERT (gRegisterOffsets[RegNumber].Size <= sizeof (Value));

  // Two characters for every byte.
  if (AsciiStrLen (Input) < gRegisterOffsets[RegNumber].Size * 2) {
    return NULL;
  }

  if (gRegisterOffsets[RegNumber].Offset != REG_NOT_PRESENT) {
    RegPtr = Registers + gRegisterOffsets[RegNumber].Offset;
    ZeroMem (&Value[0], sizeof (Value));
    for (Index = 0; Index < gRegisterOffsets[RegNumber].Size; Index += 1) {
      Value[Index] = HexToByte (Input);
      Input       += 2;
    }

    CopyMem (RegPtr, &Value[0], gRegisterOffsets[RegNumber].Size);
  }

  return Input;
}

/**
  Processes a GDB general registers write command.

  @param[in] Command  The GDB command.

**/
VOID
WriteGeneralRegisters (
  IN  CHAR8  *Data
  )
{
  CHAR8  *Ptr;
  UINTN  i;

  Ptr = &Data[0];
  for (i = 0; i < gRegisterCount; i++) {
    // Use SystemContextX64 generically, This is a union of all pointers.
    Ptr = WriteRegisterToContext ((UINT8 *)gSystemContext->SystemContextX64, i, Ptr);
    if (Ptr == NULL) {
      SendGdbError (GDB_ERROR_INTERNAL);
      return;
    }
  }

  SendGdbResponse ("OK");
}

/**
  Processes a GDB general register read command.

**/
VOID
ReadGeneralRegisters (
  )
{
  CHAR8  *Ptr;
  UINTN  i;

  Ptr = &mResponse[0];
  for (i = 0; i < gRegisterCount; i++) {
    // Use SystemContextX64 generically, This is a union of all pointers.
    Ptr = ReadRegisterFromContext ((UINT8 *)gSystemContext->SystemContextX64, i, Ptr);
  }

  *Ptr = 0;
  SendGdbResponse (&mResponse[0]);
}

/**
  Processes a GDB register read command.

  @param[in] Command  The register read command.

**/
VOID
ReadRegister (
  CHAR8  *Command
  )
{
  CHAR8  *Ptr;
  UINTN  RegisterIndex;

  // Get the register index.
  RegisterIndex = AsciiStrHexToUintn (Command);
  if (RegisterIndex >= gRegisterCount) {
    SendGdbError (GDB_ERROR_BAD_REG_INDEX);
    return;
  }

  // Read the register. Use SystemContextX64 generically, This is a union of all pointers.
  Ptr  = &mResponse[0];
  Ptr  = ReadRegisterFromContext ((UINT8 *)gSystemContext->SystemContextX64, RegisterIndex, Ptr);
  *Ptr = 0;

  SendGdbResponse (&mResponse[0]);
}

/**
  Processes a GDB register command.

  @param[in] Command  The register write command.

**/
VOID
WriteRegister (
  CHAR8  *Command
  )
{
  CHAR8  *ValueStr;
  UINTN  RegisterIndex;
  CHAR8  *Ptr;

  ValueStr = ScanMem8 (&Command[0], AsciiStrLen (Command), '=');
  if (ValueStr == NULL) {
    SendGdbError (GDB_ERROR_BAD_REQUEST);
    return;
  }

  *ValueStr = 0;
  ValueStr++;

  // Get the register index.
  RegisterIndex = AsciiStrHexToUintn (Command);
  if (RegisterIndex >= gRegisterCount) {
    SendGdbError (GDB_ERROR_BAD_REG_INDEX);
    return;
  }

  // Use SystemContextX64 generically, This is a union of all pointers.
  Ptr = WriteRegisterToContext ((UINT8 *)gSystemContext->SystemContextX64, RegisterIndex, ValueStr);
  if (Ptr == NULL) {
    SendGdbError (GDB_ERROR_INTERNAL);
    return;
  }

  SendGdbResponse ("OK");
}

/**
  Processes a GDB breakpoint command.

  @param[in]  Remove   Indicates this is a remove breakpoint command, otherwise
                       assumes the breakpoint is being added.
  @param[in]  Command  The breakpoint command string.

**/
VOID
ProcessBreakpoint (
  BOOLEAN  Remove,
  CHAR8    *Command
  )
{
  CHAR8    *TypeStr;
  UINTN    Type;
  CHAR8    *AddressStr;
  UINTN    Address;
  CHAR8    *LengthStr;
  BOOLEAN  Result;

  TypeStr    = Command;
  AddressStr = ScanMem8 (&Command[0], AsciiStrLen (TypeStr), ',');
  if (AddressStr == NULL) {
    SendGdbError (GDB_ERROR_BAD_REQUEST);
    return;
  }

  *AddressStr = 0;
  AddressStr++;
  LengthStr = ScanMem8 (&AddressStr[0], AsciiStrLen (AddressStr), ',');
  if (LengthStr == NULL) {
    SendGdbError (GDB_ERROR_BAD_REQUEST);
    return;
  }

  *LengthStr = 0;
  LengthStr++;

  Type    = AsciiStrHexToUintn (TypeStr);
  Address = AsciiStrHexToUintn (AddressStr);
  // Length  = AsciiStrHexToUintn (LengthStr);

  // Length is currently ignored as a SW breakpoint will be a fixed length per
  // arch.

  if (Type != 0) {
    SendGdbError (GDB_ERROR_UNSUPPORTED);
    return;
  }

  if (Remove) {
    Result = RemoveSoftwareBreakpoint (Address);
  } else {
    Result = AddSoftwareBreakpoint (Address);
  }

  if (Result) {
    SendGdbResponse ("OK");
  } else {
    SendGdbError (GDB_ERROR_INTERNAL);
  }
}

/**
  Routes a validated GDB command to the appropriate routine.

  @param[in] GdbCommand        The command string between the leading $ and the
                               checksum indicator #
  @param[in] GdbCommandLength  The length of the GDB command string.

**/
STATIC
VOID
ExecuteGdbCommand (
  CHAR8   *GdbCommand,
  UINT32  GdbCommandLength
  )
{
  // DEBUG ((DEBUG_INFO, "Processing '%a' \n", GdbCommand));
  switch (GdbCommand[0]) {
    case 'g': // Read General Registers
      ReadGeneralRegisters ();
      break;

    case 'G': // Write General Registers
      WriteGeneralRegisters (&GdbCommand[1]);
      break;

    case 'p': // Read General Register
      ReadRegister (&GdbCommand[1]);
      break;

    case 'P': // Write General Register
      WriteRegister (&GdbCommand[1]);
      break;

    case 'm': // Read Memory
      ProcessMemoryCommand (FALSE, &GdbCommand[1], GdbCommandLength - 1);
      break;

    case 'M': // Write Memory
      ProcessMemoryCommand (TRUE, &GdbCommand[1], GdbCommandLength - 1);
      break;

    case 'v': // v command, needs further parsing.
      ProcessVCommand (&GdbCommand[1], GdbCommandLength - 1);
      break;

    case 'q': // General query command
      ProcessQuery (&GdbCommand[1]);
      break;

    case 'H': // Switch to thread ID
      // Nothing to do, respond OK.
      SendGdbResponse ("OK");
      break;

    case '?': // Stop reason request
      SendStopReply ();
      break;

    case '!': // Enable extended mode
      SendGdbResponse ("OK");
      break;

    case 'Z': // Insert breakpoint
      ProcessBreakpoint (FALSE, &GdbCommand[1]);
      break;

    case 'z': // Remove breakpoint
      ProcessBreakpoint (TRUE, &GdbCommand[1]);
      break;

    case 'r': // Reboot
    case 'R': // Reboot
      DebugReboot ();
      // If it returns then it didn't work.
      SendGdbError (GDB_ERROR_UNSUPPORTED);
      break;

    default:
      SendGdbError (GDB_ERROR_UNKOWN_CMD);
  }

  return;
}

/**
  Parses, and validates to a GDB packet loaded into mPacket. If this is a valid
  packet, then the command will be executed.

**/
STATIC
VOID
ProcessGdbPacket (
  IN CHAR8  *Packet,
  IN UINTN  PacketLength
  )
{
  UINT32      ChecksumIndex;
  CHAR8       ChecksumString[3];
  UINT8       Checksum;
  UINT8       ChecksumCalculated;
  EFI_STATUS  Status;

  ASSERT (Packet[0] == '$');

  //
  // Validate the checksum exists and is accurate.
  //

  for (ChecksumIndex = 1; ChecksumIndex < PacketLength; ChecksumIndex++) {
    if (Packet[ChecksumIndex] == '#') {
      break;
    }
  }

  if (ChecksumIndex + 2 >= PacketLength) {
    SendGdbAck (FALSE);
    return;
  }

  ChecksumString[0] = Packet[ChecksumIndex + 1];
  ChecksumString[1] = Packet[ChecksumIndex + 2];
  ChecksumString[2] = 0;
  Status            = AsciiStrHexToBytes (&ChecksumString[0], 2, &Checksum, sizeof (Checksum));
  if (EFI_ERROR (Status)) {
    SendGdbAck (FALSE);
    return;
  }

  ChecksumCalculated = CalculateSum8 ((UINT8 *)&Packet[1], ChecksumIndex - 1);
  if (Checksum != ChecksumCalculated) {
    SendGdbAck (FALSE);
    return;
  }

  SendGdbAck (TRUE);
  mConnectionOccurred = TRUE;

  //
  // Validated, now hand off to the parser.
  //

  Packet[ChecksumIndex] = 0;
  ExecuteGdbCommand (&Packet[1], ChecksumIndex - 1);
  return;
}

/**
  Process Serial Data

  Main processing loop for packets from the debugger

**/
STATIC
VOID
ProcessInputData (
  )
{
  UINTN  PacketLength;

  ZeroMem (&mRequest[0], sizeof (mRequest));

  for ( ; ;) {
    PacketLength = 0;
    if (!DebugReadByte ((UINT8 *)&mRequest[0], 10)) {
      goto ErrorExit;
    }

    if ((mRequest[0] == '-') && !mResponseAcknowledged) {
      SendGdbResponse (NULL);
      continue;
    } else if (mRequest[0] == '+') {
      mResponseAcknowledged = TRUE;
      continue;
    } else if (mRequest[0] != '$') {
      // If this is not the beginning of the GDB packet, throw it away.
      continue;
    }

    PacketLength++;
    do {
      if (PacketLength >= sizeof (mRequest)) {
        SendGdbAck (FALSE);
        goto ErrorExit;
      }

      if (!DebugReadByte ((UINT8 *)&mRequest[PacketLength], 1000)) {
        SendGdbAck (FALSE);
        goto ErrorExit;
      }

      PacketLength++;

      // Packet will always end in #NN, look for that pattern.
    } while ((PacketLength < 4) || (mRequest[PacketLength - 3] != '#'));

    ProcessGdbPacket (&mRequest[0], PacketLength);
  }

ErrorExit:
  return;
}

/**
  Polls for input from the debugger.

**/
VOID
DebuggerPollInput (
  VOID
  )
{
  UINT8  Character;

  while (DebugTransportPoll ()) {
    if (!DebugReadByte (&Character, 10)) {
      break;
    }

    // Check for the break character CTRL-C.
    if (Character == 0x3) {
      DebuggerBreak (BreakpointReasonDebuggerBreak);
    }
  }
}

/**
  Calls the initial breakpoint to check for debugger connection.

  @param[in]  Timeout  The number of milliseconds to wait for debugger connection.
                       If 0, then there will be no timeout.

**/
VOID
DebuggerInitialBreakpoint (
  UINT64  Timeout
  )
{
  mNextBreakpointTimeout = Timeout;
  DebuggerBreak (BreakpointReasonInitial);
}

/**
  Starts the debugger stub from within the exception handler. Will notify the
  debugger and await debug commands.

  @param[in]      ExceptionInfo  Supplies the architecture agnostic exception
                                   information.
  @param[in,out]  SystemContext    The architecture specific context structures.

**/
VOID
ReportEntryToDebugger (
  IN EXCEPTION_INFO          *ExceptionInfo,
  IN OUT EFI_SYSTEM_CONTEXT  SystemContext
  )
{
  UINT64  EndTime;

  EndTime        = 0;
  gSystemContext = &SystemContext;
  gExceptionInfo = ExceptionInfo;
  gRunning       = FALSE;

  // Squelch logging output, it can confuse the debugger.
  TransportLogSuspend ();

  // Check if there needs to be a timeout.
  if ((ExceptionInfo->ExceptionType == ExceptionBreakpoint) &&
      (mNextBreakpointTimeout != 0))
  {
    EndTime                = DebugGetTimeMs () + mNextBreakpointTimeout;
    mNextBreakpointTimeout = 0;
  }

  // Notify the debugger of the break.
  SendStopReply ();

  // Keep reading requests until one resumes execution.
  while (!gRunning) {
    while (DebugTransportPoll ()) {
      CpuPause ();
      ProcessInputData ();
    }

    // Check if there is a timeout.
    if (!mConnectionOccurred && (EndTime != 0) && (DebugGetTimeMs () >= EndTime)) {
      gRunning = TRUE;
    }
  }

  if (mRebootOnContinue) {
    DebugReboot ();
  }

  // Re-enable logging prints.
  TransportLogResume ();
}

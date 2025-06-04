/*++

Copyright (c) Microsoft Corporation.

SPDX-License-Identifier: BSD-2-Clause-Patent

Module Name:

    uefiext.cpp

Abstract:

    This file contains core UEFI debug commands.

--*/

#include "uefiext.h"
#include <vector>

UEFI_ENV  gUefiEnv = DXE;
ULONG     g_TargetMachine;

HRESULT
NotifyOnTargetAccessible (
  PDEBUG_CONTROL  Control
  )
{
  //
  // Attempt to determine what environment the debugger is in.
  //

  return S_OK;
}

const struct _DML_COLOR_MAP {
  CHAR     *Bg;
  CHAR     *Fg;
  ULONG    Mask;
} DmlColorMap[ColorMax] = {
  { "normbg", "normfg",  DEBUG_OUTPUT_NORMAL  }, // Normal
  { "verbbg", "verbfg",  DEBUG_OUTPUT_VERBOSE }, // Verbose
  { "warnbg", "warnfg",  DEBUG_OUTPUT_WARNING }, // Warning
  { "errbg",  "errfg",   DEBUG_OUTPUT_ERROR   }, // Error
  { "subbg",  "subfg",   DEBUG_OUTPUT_NORMAL  }, // Subdued
  { "normbg", "srccmnt", DEBUG_OUTPUT_NORMAL  }, // Header
  { "empbg",  "emphfg",  DEBUG_OUTPUT_NORMAL  }, // Emphasized
  { "normbg", "changed", DEBUG_OUTPUT_NORMAL  }, // Changed
};

VOID
PrintDml (
  __in PRINTF_DML_COLOR  Color,
  __in PCSTR             Format,
  ...
  )

/*++

Routine Description:

    This routine prints a string with DML markup to the debugger, optionally
    encoding the string with the given color information.

Arguments:

    Color - A color of type PRINTF_DML_COLOR. Certain colors, such as Verbose,
            Warning, and Error are given special handling by the debugger.

    Format - Format string.

    ... - Additional arguments to support the format string.

Return Value:

    None.

--*/
{
  va_list  Args;
  ULONG    Mask;

  va_start (Args, Format);
  Mask = DEBUG_OUTPUT_NORMAL;

  if ((Color > Normal) && (Color < ColorMax)) {
    Mask = DmlColorMap[Color].Mask;
    g_ExtControl->ControlledOutput (
                    DEBUG_OUTCTL_AMBIENT_DML,
                    Mask,
                    "<col fg=\"%s\" bg=\"%s\">",
                    DmlColorMap[Color].Fg,
                    DmlColorMap[Color].Bg
                    );
  }

  g_ExtControl->ControlledOutputVaList (
                  DEBUG_OUTCTL_AMBIENT_DML,
                  Mask,
                  Format,
                  Args
                  );

  if ((Color > Normal) && (Color < ColorMax)) {
    g_ExtControl->ControlledOutput (
                    DEBUG_OUTCTL_AMBIENT_DML,
                    Mask,
                    "</col>"
                    );
  }

  va_end (Args);
}

HRESULT CALLBACK
setenv (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  INIT_API ();

  if (_stricmp (args, "PEI") == 0) {
    gUefiEnv = PEI;
  } else if (_stricmp (args, "DXE") == 0) {
    gUefiEnv = DXE;
  } else if (_stricmp (args, "MM") == 0) {
    gUefiEnv = MM;
  } else if (_stricmp (args, "rust") == 0) {
    gUefiEnv = RUST;
  } else {
    dprintf ("Unknown environment type! Supported types: PEI, DXE, MM, rust\n");
  }

  EXIT_API ();
  return S_OK;
}

HRESULT CALLBACK
help (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  INIT_API ();

  UNREFERENCED_PARAMETER (args);

  dprintf (
    "Help for uefiext.dll\n"
    "\nBasic Commands:\n"
    "  help                - Shows this help\n"
    "  init                - Detects and initializes windbg for debugging UEFI.\n"
    "  setenv              - Set the extensions environment mode\n"
    "\nModule Discovery:\n"
    "  findall             - Attempts to detect environment and load all modules\n"
    "  findmodule          - Find the currently running module\n"
    "  elf                 - Dumps the headers of an ELF image\n"
    "\nData Parsing:\n"
    "  memorymap           - Prints the current memory map\n"
    "  hobs                - Enumerates the hand off blocks\n"
    "  protocols           - Lists the protocols from the protocol list.\n"
    "  pt                  - Dumps the page tables for a given address\n"
    "  handles             - Prints the handles list.\n"
    "  linkedlist          - Parses a UEFI style linked list of entries.\n"
    "  efierror            - Translates an EFI error code.\n"
    "  advlog              - Prints the advanced logger memory log.\n"
    "\nUEFI Debugger:\n"
    "  info                - Queries information about the UEFI debugger\n"
    "  monitor             - Sends direct monitor commands\n"
    "  modulebreak         - Sets a break on load for the provided module. e.g. 'shell'\n"
    "  readmsr             - Reads a MSR value (x86 only)\n"
    "  readvar             - Reads a UEFI variable\n"
    "  reboot              - Reboots the system\n"
    );

  EXIT_API ();

  return S_OK;
}

HRESULT CALLBACK
uefiext_init (
  PDEBUG_CLIENT4  Client,
  PCSTR           args
  )
{
  ULONG  TargetClass = 0;
  ULONG  TargetQual  = 0;
  ULONG  Mask;
  PCSTR  Output;

  INIT_API ();

  UNREFERENCED_PARAMETER (args);

  dprintf ("Initializing UEFI Debugger Extension\n");
  g_ExtControl->GetDebuggeeType (&TargetClass, &TargetQual);
  if ((TargetClass == DEBUG_CLASS_KERNEL) && (TargetQual == DEBUG_KERNEL_EXDI_DRIVER)) {
    // Enabled the verbose flag in the output mask. This is required for .exdicmd
    // output.
    Client->GetOutputMask (&Mask);
    Client->SetOutputMask (Mask | DEBUG_OUTPUT_VERBOSE);

    if ((Status = g_ExtControl->GetActualProcessorType (&g_TargetMachine)) != S_OK) {
      return S_FALSE;
    }

    if ((Status = Client->QueryInterface (__uuidof (IDebugRegisters), (void **)&g_ExtRegisters)) != S_OK) {
      return S_FALSE;
    }

    // Detect if this is a UEFI software debugger.
    Output = ExecuteCommandWithOutput (Client, ".exdicmd target:0:?");
    if (strstr (Output, "Rust Debugger") != NULL) {
      dprintf ("Rust UEFI Debugger detected.\n");
      gUefiEnv = RUST;
    } else if (strstr (Output, "DXE UEFI Debugger") != NULL) {
      dprintf ("DXE UEFI Debugger detected.\n");
      gUefiEnv = DXE;
    } else {
      dprintf ("Unknown environment, assuming DXE.\n");
      gUefiEnv = DXE;
    }

    dprintf ("Scanning for images.\n");
    if (gUefiEnv == DXE || gUefiEnv == RUST) {
      g_ExtControl->Execute (
                      DEBUG_OUTCTL_ALL_CLIENTS,
                      "!uefiext.findall",
                      DEBUG_EXECUTE_DEFAULT
                      );
    } else {
      g_ExtControl->Execute (
                      DEBUG_OUTCTL_ALL_CLIENTS,
                      "!uefiext.findmodule",
                      DEBUG_EXECUTE_DEFAULT
                      );
    }
  }

  EXIT_API ();

  return S_OK;
}

// Used to capture output from debugger commands
std::vector<std::string>  mResponses = { };

class OutputCallbacks : public IDebugOutputCallbacks {
public:

  STDMETHOD (QueryInterface)(THIS_ REFIID InterfaceId, PVOID *Interface) {
    if (InterfaceId == __uuidof (IDebugOutputCallbacks)) {
      *Interface = (IDebugOutputCallbacks *)this;
      AddRef ();
      return S_OK;
    } else {
      *Interface = NULL;
      return E_NOINTERFACE;
    }
  }

  STDMETHOD_ (ULONG, AddRef)(THIS) {
    return 1;
  }

  STDMETHOD_ (ULONG, Release)(THIS) {
    return 1;
  }

  STDMETHOD (Output)(THIS_ ULONG Mask, PCSTR Text) {
    mResponses.push_back (std::string(Text));
    return S_OK;
  }
};

OutputCallbacks  mOutputCallback;

PSTR
ExecuteCommandWithOutput (
  PDEBUG_CLIENT4  Client,
  PCSTR           Command
  )
{
  PDEBUG_OUTPUT_CALLBACKS  Callbacks;
  static CHAR *Output = NULL;
  SIZE_T Offset;
  SIZE_T Length;

  // To avoid complexity of tracking a shifting buffer, easier to just lazily free here.
  if (Output != NULL) {
    free (Output);
    Output = NULL;
  }

  mResponses.clear ();

  Client->GetOutputCallbacks (&Callbacks);
  Client->SetOutputCallbacks (&mOutputCallback);
  g_ExtControl->Execute (
                  DEBUG_OUTCTL_ALL_CLIENTS,
                  Command,
                  DEBUG_EXECUTE_DEFAULT
                  );
  Client->SetOutputCallbacks (Callbacks);

  Length = 0;
  for (const auto &str : mResponses) {
    Length += str.length ();
  }

  Output = (CHAR *)malloc (Length + 1);
  Offset = 0;
  for (const auto &str : mResponses) {
    memcpy (Output + Offset, str.c_str (), str.length ());
    Offset += str.length ();
  }

  Output[Offset] = '\0';
  return Output;
}

PSTR
MonitorCommandWithOutput (
  PDEBUG_CLIENT4  Client,
  PCSTR           MonitorCommand,
  ULONG           Offset
  )
{
  CHAR   Command[512];
  PSTR   Output;
  ULONG  Mask;
  PCSTR  Preamble = "Target command response: ";
  PCSTR  Ending   = "exdiCmd:";
  PCSTR  Ok       = "OK\n";

  if (Offset == 0 ) {
    sprintf_s (Command, sizeof (Command), ".exdicmd target:0:%s", MonitorCommand);
  } else {
    sprintf_s (Command, sizeof (Command), ".exdicmd target:0:O[%d] %s", Offset, MonitorCommand);
  }

  Client->GetOutputMask (&Mask);
  Client->SetOutputMask (Mask | DEBUG_OUTPUT_VERBOSE);
  Output = ExecuteCommandWithOutput (Client, Command);
  Client->SetOutputMask (Mask);

  // Clean up the output.
  if (strstr (Output, Preamble) != NULL) {
    Output = strstr (Output, Preamble) + strlen (Preamble);
  }

  if (strstr (Output, Ending) != NULL) {
    *strstr (Output, Ending) = 0;
  }

  // If it has the OK string appended to the end, remove it
  if (strlen(Output) > strlen (Ok)) {
    size_t Offset = strlen (Output) - strlen (Ok);
    if (strcmp (Output + Offset, Ok) == 0) {
      strcpy_s(Output + Offset, strlen (Ok) + 1, "\n");
    }
  }

  return Output;
}

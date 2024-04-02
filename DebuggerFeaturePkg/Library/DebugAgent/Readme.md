# Debug Agent

The debug agent modules implement the core of the debugger. The agent is responsible
for installing the debug exception handlers and communicating with the debugger
when a break occurs or periodically through polling.

## Architecture

The structure of the debug agent is broken into several modules to handle different
roles of the debugger. Each of these code segments are designed to be extensible
such that a new implementation of one does not require knowledge from the other
sections. For example, adding a new architecture should require minimal changes
to the core or protocol modules. In the future these could be divided into libraries
if multiple implementations of components such as the protocol file are needed.

### Debug Agent Core

This is the phase specific code (e.g. [DebugAgentDxe.c](./DebugAgentDxe.c)) that
is responsible for initializing the debugger and handling any phase specific operations
such as timer polling, boot/runtime service access, etc.

### Architecture Code

This code is responsible for handling all architecture specific logic, such as
implementing the exception handler, page table walking, architecture debugging
specific initialization/handling. This code should not be phase or protocol
specific, but is generally applicable to the architecture. An example of this is
[DebugAarch64.c](./AARCH64/DebugAarch64.c).

### Protocol

This code implements the protocol for communicating with the debugger application.
Currently the only implementation is the [GdbStub](./GdbStub) which also has some
architecture specific files to handle accessing architecture specific information
like registers.

## Copyright

Copyright (C) Microsoft Corporation. All rights reserved.

SPDX-License-Identifier: BSD-2-Clause-Patent

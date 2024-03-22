//! GDB Stub Manager
//!
//! Implements the core debugging logic and interfaces between common debugger
//! code and the GdbStub crate.
//!
//! ## Examples and Usage
//!
//! ## License
//!
//! Copyright (C) Microsoft Corporation. All rights reserved.
//!
//! SPDX-License-Identifier: BSD-2-Clause-Patent
//!
#![no_std]

use core::panic::PanicInfo;
// use lazy_static::lazy_static;

use gdbstub::stub::{state_machine::GdbStubStateMachine, GdbStubBuilder, SingleThreadStopReason};

mod connection;
mod target;

// static TARGET: target::EfiTarget = target::EfiTarget {};
// static CONNECTION: connection::DebuggerTransport = connection::DebuggerTransport { timeout: 10, peek_value: None };

// lazy_static! {
//   static
// }

// TODO lazy static.

#[no_mangle]
pub extern "C" fn GdbInitialize() {}

#[no_mangle]
pub extern "C" fn GdbBreak() {
  let mut buf: [u8; 4096] = [0; 4096];
  let mut conn: connection::DebuggerTransport = connection::DebuggerTransport { timeout: 10, peek_value: None };
  let mut target: target::EfiTarget = target::EfiTarget {};

  let gdb_res: Result<
    gdbstub::stub::GdbStub<'_, target::EfiTarget, connection::DebuggerTransport>,
    gdbstub::stub::GdbStubBuilderError,
  > = GdbStubBuilder::new(conn).with_packet_buffer(&mut buf).build();

  let gdb = gdb_res.unwrap();
  let mut gdb = gdb.run_state_machine(&mut target).unwrap();

  let res = loop {
    gdb = match gdb {
      GdbStubStateMachine::Idle(mut gdb) => {
        let mut byte = gdb.borrow_conn().read_blocking();
        match gdb.incoming_data(&mut target, byte) {
          Ok(gdb) => gdb,
          Err(e) => break Err(e),
        }
      }
      GdbStubStateMachine::Running(gdb) => match gdb.report_stop(&mut target, SingleThreadStopReason::DoneStep) {
        Ok(gdb) => gdb,
        Err(e) => break Err(e),
      },
      GdbStubStateMachine::CtrlCInterrupt(gdb) => {
        match gdb.interrupt_handled(&mut target, None::<SingleThreadStopReason<u64>>) {
          Ok(gdb) => gdb,
          Err(e) => break Err(e),
        }
      }
      GdbStubStateMachine::Disconnected(gdb) => break Ok(gdb.get_reason()),
    }
  };
}

#[no_mangle]
pub extern "C" fn GdbPoll() {}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
  loop {}
}

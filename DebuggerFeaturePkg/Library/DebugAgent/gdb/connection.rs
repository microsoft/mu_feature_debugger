//! Implementaion of the GdbStub connection to link to DebugTransportLib
//!
//! Implements the core debugging logic and interfaces between common debugger
//! code and the GdbStub crate.
//!
//! ## License
//!
//! Copyright (C) Microsoft Corporation. All rights reserved.
//!
//! SPDX-License-Identifier: BSD-2-Clause-Patent
//!

use gdbstub::conn::{Connection, ConnectionExt};
use r_efi::efi::Status;

// Externs for DebugTransportLib.h
extern "C" {
  fn DebugTransportInitialize() -> Status;
  fn DebugTransportRead(Buffer: *mut u8, NumberOfBytes: usize, Timeout: usize) -> usize;
  fn DebugTransportWrite(Buffer: *const u8, NumberOfBytes: usize) -> usize;
  fn DebugTransportPoll() -> bool;
}

pub struct DebuggerTransport {
  pub timeout: usize,
  pub peek_value: Option<u8>,
}

impl DebuggerTransport {
  pub fn initialize() -> Status {
    let status = unsafe { DebugTransportInitialize() };
    return status;
  }

  pub fn poll(&mut self) -> bool {
    return unsafe { DebugTransportPoll() };
  }

  pub fn read(&mut self) -> Result<u8, &'static str> {
    if let Some(peek_byte) = self.peek_value {
      self.peek_value = None;
      return Ok(peek_byte);
    }

    let mut b: u8 = 0;
    let read = unsafe { DebugTransportRead(&mut b, 1, self.timeout) };
    if read == 1 {
      Ok(b)
    } else {
      Err("No data read")
    }
  }

  pub fn read_blocking(&mut self) -> u8 {
    while !self.poll() {}

    let mut b: u8 = 0;
    let read = unsafe { DebugTransportRead(&mut b, 1, self.timeout) };
    b
  }
}

impl Connection for DebuggerTransport {
  type Error = &'static str;

  fn write(&mut self, b: u8) -> Result<(), Self::Error> {
    let written = unsafe { DebugTransportWrite(&b, 1) };

    if written == 1 {
      Ok(())
    } else {
      Err("Transport write failed")
    }
  }

  fn write_all(&mut self, buf: &[u8]) -> Result<(), Self::Error> {
    let written = unsafe { DebugTransportWrite(buf.as_ptr(), buf.len()) };

    if written == buf.len() {
      Ok(())
    } else if written > 0 {
      Err("Data partially written")
    } else {
      Err("No data written")
    }
  }

  fn flush(&mut self) -> Result<(), Self::Error> {
    // No flush for the debug transport.
    Ok(())
  }
}

// impl ConnectionExt for DebuggerTransport {
//   fn read(&mut self) -> Result<u8, Self::Error> {
//     if let Some(peek_byte) = self.peek_value {
//       self.peek_value = None;
//       return Ok(peek_byte);
//     }

//     let mut b: u8 = 0;
//     let read = unsafe { DebugTransportRead(&mut b, 1, self.timeout) };
//     if read == 1 {
//       Ok(b)
//     } else {
//       Err("No data read")
//     }
//   }

//   fn peek(&mut self) -> Result<Option<u8>, Self::Error> {
//     if self.peek_value.is_none() {
//       let ready = unsafe { DebugTransportPoll() };
//       if ready {
//         let b = self.read()?;
//         self.peek_value = Some(b);
//       }
//     }

//     return Ok(self.peek_value);
//   }
// }

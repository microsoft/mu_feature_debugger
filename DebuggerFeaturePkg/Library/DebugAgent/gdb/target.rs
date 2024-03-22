use gdbstub::arch::Arch;
use gdbstub::common::Signal;
use gdbstub::target;
use gdbstub::target::ext::base::singlethread;
use gdbstub::target::ext::base::singlethread::SingleThreadBase;
use gdbstub::target::ext::base::singlethread::SingleThreadResume;
use gdbstub::target::Target;
use gdbstub::target::TargetError;
use gdbstub::target::TargetResult;
use target::ext;
use target::ext::base::BaseOps;
use target::ext::breakpoints;

#[cfg(target_arch = "aarch64")]
use gdbstub_arch::aarch64::AArch64;
#[cfg(target_arch = "x86_64")]
use gdbstub_arch::x86::X86_64_SSE;

// Externs for DebugAgent.h
extern "C" {
  fn DbgReadMemory(address: u64, data: *mut u8, size: usize) -> bool;
  fn DbgWriteMemory(address: u64, data: *const u8, size: usize) -> bool;
  fn AddSoftwareBreakpoint(address: u64) -> bool;
  fn RemoveSoftwareBreakpoint(address: u64) -> bool;
}

pub struct EfiTarget {}

impl Target for EfiTarget {
  #[cfg(target_arch = "x86_64")]
  type Arch = X86_64_SSE;
  #[cfg(target_arch = "aarch64")]
  type Arch = AArch64;

  type Error = &'static str;

  #[inline(always)]
  fn base_ops(&mut self) -> target::ext::base::BaseOps<'_, Self::Arch, Self::Error> {
    BaseOps::SingleThread(self)
  }

  // With a shared serial line, take no chances.
  #[inline(always)]
  fn use_no_ack_mode(&self) -> bool {
    false
  }

  #[inline(always)]
  fn support_breakpoints(&mut self) -> Option<target::ext::breakpoints::BreakpointsOps<'_, Self>> {
    Some(self)
  }

  // windbg only understand basic memory packets.
  #[inline(always)]
  fn use_x_upcase_packet(&self) -> bool {
    false
  }

  // Support custom monitor commands.
  #[inline(always)]
  fn support_monitor_cmd(&mut self) -> Option<ext::monitor_cmd::MonitorCmdOps<'_, Self>> {
    None
  }
}

impl SingleThreadBase for EfiTarget {
  fn read_registers(&mut self, regs: &mut <Self::Arch as gdbstub::arch::Arch>::Registers) -> TargetResult<(), Self> {
    todo!()
  }

  fn write_registers(&mut self, regs: &<Self::Arch as gdbstub::arch::Arch>::Registers) -> TargetResult<(), Self> {
    todo!()
  }

  fn read_addrs(
    &mut self,
    start_addr: <Self::Arch as gdbstub::arch::Arch>::Usize,
    data: &mut [u8],
  ) -> TargetResult<usize, Self> {
    let res = unsafe { DbgReadMemory(start_addr, data.as_mut_ptr(), data.len()) };
    if res {
      Ok(data.len())
    } else {
      Err(TargetError::NonFatal)
    }
  }

  fn write_addrs(
    &mut self,
    start_addr: <Self::Arch as gdbstub::arch::Arch>::Usize,
    data: &[u8],
  ) -> TargetResult<(), Self> {
    let res = unsafe { DbgWriteMemory(start_addr, data.as_ptr(), data.len()) };
    if res {
      Ok(())
    } else {
      Err(TargetError::NonFatal)
    }
  }

  // #[inline(always)]
  // fn support_resume(&mut self) -> Option<singlethread::SingleThreadResumeOps<'_, Self>> {
  //   Some(self)
  // }
}

impl breakpoints::Breakpoints for EfiTarget {
  #[inline(always)]
  fn support_sw_breakpoint(&mut self) -> Option<breakpoints::SwBreakpointOps<'_, Self>> {
    Some(self)
  }
}

impl breakpoints::SwBreakpoint for EfiTarget {
  fn add_sw_breakpoint(&mut self, addr: u64, _kind: <Self::Arch as Arch>::BreakpointKind) -> TargetResult<bool, Self> {
    let res = unsafe { AddSoftwareBreakpoint(addr) };
    Ok(res)
  }

  fn remove_sw_breakpoint(&mut self, addr: u64, _kind: usize) -> TargetResult<bool, Self> {
    let res = unsafe { AddSoftwareBreakpoint(addr) };
    Ok(res)
  }
}

// impl SingleThreadResume for EfiTarget {
//     fn resume(&mut self, signal: Option<Signal>) -> Result<(), Self::Error> {
//         todo!()
//     }
// }

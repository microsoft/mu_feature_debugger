## @file
#
# This Package provides all definitions and library instances for the
# Debugger Feature Package.
#
# Copyright (c) Microsoft Corporation. All rights reserved.
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

[Defines]
  PLATFORM_NAME                  = DebuggerFeaturePkg
  PLATFORM_GUID                  = bfeb8921-365e-43a4-866a-6c52ea6955e3
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/DebuggerFeaturePkg
  SUPPORTED_ARCHITECTURES        = IA32|X64|AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT

[LibraryClasses]
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  PeimEntryPoint|MdePkg/Library/PeimEntryPoint/PeimEntryPoint.inf
  HobLib|MdeModulePkg/Library/BaseHobLibNull/BaseHobLibNull.inf
  TimerLib|MdePkg/Library/BaseTimerLibNullTemplate/BaseTimerLibNullTemplate.inf
  DeviceStateLib|MdeModulePkg/Library/DeviceStateLib/DeviceStateLib.inf
  RegisterFilterLib|MdePkg/Library/RegisterFilterLibNull/RegisterFilterLibNull.inf

  DebugTransportLib|DebuggerFeaturePkg/Library/DebugTransportSerialLib/DebugTransportSerialLib.inf
  WatchdogTimerLib|DebuggerFeaturePkg/Library/WatchdogTimerLibNull/WatchdogTimerLibNull.inf
  TransportLogControlLib|DebuggerFeaturePkg/Library/TransportLogControlLibNull/TransportLogControlLibNull.inf

  StackCheckLib|MdePkg/Library/StackCheckLibNull/StackCheckLibNull.inf

[LibraryClasses.common.PEIM]
  MemoryAllocationLib|MdePkg/Library/PeiMemoryAllocationLib/PeiMemoryAllocationLib.inf
  PeiServicesLib|MdePkg/Library/PeiServicesLib/PeiServicesLib.inf
  PeiServicesTablePointerLib|MdePkg/Library/PeiServicesTablePointerLib/PeiServicesTablePointerLib.inf

[Components]
  DebuggerFeaturePkg/DebugConfigPei/DebugConfigPei.inf
  DebuggerFeaturePkg/Library/DebugTransportSerialLib/DebugTransportSerialLib.inf
  DebuggerFeaturePkg/Library/WatchdogTimerLibNull/WatchdogTimerLibNull.inf
  DebuggerFeaturePkg/Library/TransportLogControlLibNull/TransportLogControlLibNull.inf

[Components.X64, Components.AARCH64]
  DebuggerFeaturePkg/Library/DebugAgent/DebugAgentDxe.inf
  DebuggerFeaturePkg/Library/DebugAgent/DebugAgentMm.inf

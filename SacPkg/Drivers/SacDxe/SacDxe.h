/** @file
  SAC device support.

  Copyright (C) 2023 Andrei Warkentin. All rights reserved.<BR>
  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#pragma once

#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/SerialIo.h>
#include <Protocol/DevicePath.h>
#include <Protocol/PciIo.h>
#include <Protocol/ComponentName.h>
#include <Protocol/DriverBinding.h>
#include <IndustryStandard/Pci.h>
#include <Guid/SerialPortLibVendor.h>
#include <SacRegs.h>

#define SAC_SIGNATURE  SIGNATURE_32('S','A','C','D')

#define SERIAL_PORT_DEFAULT_TIMEOUT  1000000

extern EFI_COMPONENT_NAME_PROTOCOL   gSacComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL  gSacComponentName2;
extern EFI_DRIVER_BINDING_PROTOCOL   gSacDriverBinding;

#define SAC_PRIVATE_FROM_SIO(a)  CR(a, SAC_PRIVATE_DATA, SerialIo, SAC_SIGNATURE)

typedef struct {
  UINT32                    Signature;
  EFI_PCI_IO_PROTOCOL       *PciIo;
  EFI_SERIAL_IO_PROTOCOL    SerialIo;
  EFI_SERIAL_IO_MODE        SerialMode;
  UINT32                    ReadData;
} SAC_PRIVATE_DATA;

VOID
SacConWrite (
  IN  SAC_PRIVATE_DATA  *Private,
  IN  UINT32            CharData
  );

UINT32
SacConRead (
  IN  SAC_PRIVATE_DATA  *Private,
  IN  BOOLEAN           WaitForData
  );

BOOLEAN
SacConPoll (
  IN  SAC_PRIVATE_DATA  *Private
  );

EFI_STATUS
EFIAPI
SacSerialReset (
  IN EFI_SERIAL_IO_PROTOCOL  *This
  );

EFI_STATUS
EFIAPI
SacSerialSetAttributes (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN UINT64                  BaudRate,
  IN UINT32                  ReceiveFifoDepth,
  IN UINT32                  Timeout,
  IN EFI_PARITY_TYPE         Parity,
  IN UINT8                   DataBits,
  IN EFI_STOP_BITS_TYPE      StopBits
  );

EFI_STATUS
EFIAPI
SacSerialSetControl (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN UINT32                  Control
  );

EFI_STATUS
EFIAPI
SacSerialGetControl (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  OUT UINT32                 *Control
  );

EFI_STATUS
EFIAPI
SacSerialWrite (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN OUT UINTN               *BufferSize,
  IN VOID                    *Buffer
  );

EFI_STATUS
EFIAPI
SacSerialRead (
  IN  EFI_SERIAL_IO_PROTOCOL  *This,
  IN  OUT UINTN               *BufferSize,
  OUT VOID                    *Buffer
  );

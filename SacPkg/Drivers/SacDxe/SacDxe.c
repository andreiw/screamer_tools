/** @file
  SAC device support.

  Copyright (C) 2023 Andrei Warkentin. All rights reserved.<BR>
  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SacDxe.h"

EFI_STATUS
EFIAPI
SacSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

EFI_STATUS
EFIAPI
SacStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

EFI_STATUS
EFIAPI
SacStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer
  );

EFI_DRIVER_BINDING_PROTOCOL  gSacDriverBinding = {
  SacSupported,
  SacStart,
  SacStop,
  0xa,
  NULL,
  NULL
};

EFI_STATUS
EFIAPI
SacSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS           Status;
  EFI_PCI_IO_PROTOCOL  *PciIo;
  UINT16               Value;

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint16,
                            PCI_VENDOR_ID_OFFSET,
                            1, &Value);
  if (EFI_ERROR (Status) || Value != 0x10ee) {
    return EFI_UNSUPPORTED;    
  }

  Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint16,
                            PCI_DEVICE_ID_OFFSET,
                            1, &Value);
  if (EFI_ERROR (Status) || Value != 0x0666) {
    return EFI_UNSUPPORTED;    
  }

  Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint16,
                            PCI_SUBSYSTEM_ID_OFFSET,
                            1, &Value);
  if (!EFI_ERROR (Status) && Value != 0x4157) {
    DEBUG ((DEBUG_ERROR,
            "Stock pcileech firmware, functionality reduced\n"));
  }

  return Status;
}

EFI_STATUS
EFIAPI
SacStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  )
{
  EFI_STATUS                        Status;
  EFI_PCI_IO_PROTOCOL               *PciIo;
  SAC_PRIVATE_DATA                  *Private;

  PciIo = NULL;
  Private = NULL;
  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (EFI_ERROR (Status)) {
    if (Status == EFI_ALREADY_STARTED) {
      return Status;
    }
  }

  Private = AllocateZeroPool (sizeof (SAC_PRIVATE_DATA));
  if (Private == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto out;
  }

  Private->Signature = SAC_SIGNATURE;
  Private->PciIo = PciIo;

  Private->SerialIo.Revision = SERIAL_IO_INTERFACE_REVISION;
  Private->SerialIo.Reset = SacSerialReset;
  Private->SerialIo.SetAttributes = SacSerialSetAttributes;
  Private->SerialIo.SetControl = SacSerialSetControl;
  Private->SerialIo.GetControl = SacSerialGetControl;
  Private->SerialIo.Read = SacSerialRead;
  Private->SerialIo.Write = SacSerialWrite;
  Private->SerialIo.Mode = &Private->SerialMode;

  Private->SerialMode.BaudRate = PcdGet64 (PcdUartDefaultBaudRate);
  Private->SerialMode.DataBits = PcdGet8 (PcdUartDefaultDataBits);
  Private->SerialMode.Parity = PcdGet8 (PcdUartDefaultParity);
  Private->SerialMode.StopBits = (UINT32)PcdGet8 (PcdUartDefaultStopBits);
  Private->SerialMode.ReceiveFifoDepth = PcdGet16 (PcdUartDefaultReceiveFifoDepth);
  Private->SerialMode.Timeout = SERIAL_PORT_DEFAULT_TIMEOUT;

  Status = gBS->InstallMultipleProtocolInterfaces (
    &Controller,
    &gEfiSerialIoProtocolGuid,
    &(Private->SerialIo),
    NULL
    );

 out:
  if (EFI_ERROR (Status)) {
    if (Private != NULL) {
      FreePool (Private);
    }
    if (PciIo != NULL) {
      gBS->CloseProtocol (Controller, &gEfiPciIoProtocolGuid,
                          This->DriverBindingHandle, Controller);
    }
  }
  return Status;
}

EFI_STATUS
EFIAPI
SacStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   Controller,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer
  )
{
  EFI_STATUS               Status;
  EFI_SERIAL_IO_PROTOCOL  *SerialIo;
  SAC_PRIVATE_DATA        *Private;

  Status = gBS->OpenProtocol (
                  Controller,
                  &gEfiSerialIoProtocolGuid,
                  (VOID **)&SerialIo,
                  This->DriverBindingHandle,
                  Controller,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Private = SAC_PRIVATE_FROM_SIO (This);

  Status = gBS->UninstallMultipleProtocolInterfaces (
    &Controller,
    &gEfiSerialIoProtocolGuid,
    &(Private->SerialIo),
    NULL
    );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  gBS->CloseProtocol (Controller, &gEfiPciIoProtocolGuid,
                      This->DriverBindingHandle, Controller);

  FreePool (Private);

  return EFI_SUCCESS;
}

VOID
SacConWrite (IN  SAC_PRIVATE_DATA *Private,
             IN  UINT32 CharData)
{
  Private->PciIo->Pci.Write (Private->PciIo,
                             EfiPciIoWidthUint32,
                             SAC_CFG_TEXT_IN, 1,
                             &CharData);
}

BOOLEAN
SacConPoll (IN  SAC_PRIVATE_DATA *Private)
{
  if (Private->ReadData != 0xffffffff) {
    return TRUE;
  }

  Private->ReadData = SacConRead (Private, FALSE);

  if (Private->ReadData != 0xffffffff) {
    return TRUE;
  }

  return FALSE;
}

UINT32
SacConRead (IN  SAC_PRIVATE_DATA *Private,
            IN  BOOLEAN WaitForData)
{
  UINT32 CharData;
  EFI_STATUS Status;

  if (Private->ReadData != 0xffffffff) {
    CharData = Private->ReadData;
    Private->ReadData = 0xffffffff;
    return CharData;
  }
  
  CharData = 0;
  do {
    Status = Private->PciIo->Pci.Read (Private->PciIo,
                                       EfiPciIoWidthUint32,
                                       SAC_CFG_TEXT_IN,
                                       1, &CharData);
    if (EFI_ERROR (Status)) {
      return 0;
    }
  } while (WaitForData && (CharData != 0xffffffff));

  return CharData;
}

EFI_STATUS
EFIAPI
EntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             &gSacDriverBinding,
             ImageHandle,
             &gSacComponentName,
             &gSacComponentName2
             );
}

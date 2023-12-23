/** @file
  Simple Console that sits on a SAC device.

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
  Private->SimpleTextOut.Reset = TextOutReset;
  Private->SimpleTextOut.OutputString = OutputString;
  Private->SimpleTextOut.TestString = TestString;
  Private->SimpleTextOut.QueryMode = QueryMode;
  Private->SimpleTextOut.SetMode = SetMode;
  Private->SimpleTextOut.SetAttribute = SetAttribute;
  Private->SimpleTextOut.ClearScreen = ClearScreen;
  Private->SimpleTextOut.SetCursorPosition = SetCursorPosition;
  Private->SimpleTextOut.EnableCursor = EnableCursor;
  Private->SimpleTextOut.Mode = &Private->SimpleTextOutMode;
  
  Private->SimpleTextIn.Reset = TextInReset;
  Private->SimpleTextIn.ReadKeyStroke = ReadKeyStroke;

  Status = gBS->CreateEvent (
                             EVT_NOTIFY_WAIT,
                             TPL_NOTIFY,
                             WaitForKeyEvent,
                             Private,
                             &Private->SimpleTextIn.WaitForKey
                             );
  if (EFI_ERROR (Status)) {
    goto out;
  }

  Private->SimpleTextOutMode.MaxMode = 1;
  Private->SimpleTextOutMode.Mode = 0;
  Private->SimpleTextOutMode.Attribute = EFI_TEXT_ATTR (EFI_LIGHTGRAY, EFI_BLACK);
  Private->SimpleTextOutMode.CursorColumn = 0;
  Private->SimpleTextOutMode.CursorRow = 0;
  Private->SimpleTextOutMode.CursorVisible = TRUE;

  Status = gBS->InstallMultipleProtocolInterfaces (
    &Controller,
    &gEfiSimpleTextOutProtocolGuid,
    &(Private->SimpleTextOut),
    &gEfiSimpleTextInProtocolGuid,
    &(Private->SimpleTextIn),
    NULL
    );

 out:
  if (EFI_ERROR (Status)) {
    if (Private != NULL) {
      if (Private->SimpleTextIn.WaitForKey != NULL) {
        gBS->CloseEvent (Private->SimpleTextIn.WaitForKey);
      }
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
  return EFI_UNSUPPORTED;
}

VOID
SacConWrite (IN  SAC_PRIVATE_DATA *Private,
             IN  UINT8 Data)
{
  Private->PciIo->Pci.Write (Private->PciIo,
                             EfiPciIoWidthUint8,
                             SAC_CFG_TEXT_IN, 1,
                             &Data);
}

BOOLEAN
SacConPoll (IN  SAC_PRIVATE_DATA *Private)
{
  UINT8 Char;
  EFI_STATUS Status;

  if (Private->ReadData != 0) {
    return TRUE;
  }

  Char = 0;
  Status = Private->PciIo->Pci.Read (Private->PciIo,
                                     EfiPciIoWidthUint8,
                                     SAC_CFG_TEXT_IN,
                                     1, &Char);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  Private->ReadData = Char;
  return TRUE;
}

UINT8
SacConRead (IN  SAC_PRIVATE_DATA *Private,
            IN  BOOLEAN WaitForData)
{
  UINT8 Char;
  EFI_STATUS Status;

  if (Private->ReadData != 0) {
    Char = Private->ReadData;
    Private->ReadData = 0;
    return Char;
  }
  
  Char = 0;
  do {
    Status = Private->PciIo->Pci.Read (Private->PciIo,
                                       EfiPciIoWidthUint8,
                                       SAC_CFG_TEXT_IN,
                                       1, &Char);
    if (EFI_ERROR (Status)) {
      return 0;
    }
  } while (WaitForData && (Char == 0 || Char == 0xFF));

  return Char;
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

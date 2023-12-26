/** @file
  SAC HelloWorld.

  Copyright (C) 2023 Andrei Warkentin. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Protocol/PciIo.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <IndustryStandard/Pci.h>
#include <SacRegs.h>

static EFI_PCI_IO_PROTOCOL  *gPciIo;

VOID
SacPrintChar (
  IN  CHAR8  Char
  )
{
  if (gPciIo == NULL) {
    return;
  }

  gPciIo->Pci.Write (
                gPciIo,
                EfiPciIoWidthUint8,
                SAC_CFG_TEXT_OUT,
                1,
                &Char
                );
}

VOID
SacPrint (
  IN  CHAR8  *String
  )
{
  while (*String != '\0') {
    if (*String == '\n') {
      SacPrintChar ('\r');
    }

    SacPrintChar (*String++);
  }
}

EFI_STATUS
EFIAPI
UefiMain (
  IN  EFI_HANDLE        ImageHandle,
  IN  EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINTN       PciIndex;
  UINTN       PciCount;
  EFI_STATUS  Status;
  EFI_HANDLE  *PciHandles;

  PciCount   = 0;
  PciHandles = NULL;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiPciIoProtocolGuid,
                  NULL,
                  &PciCount,
                  &PciHandles
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (PciIndex = 0; PciIndex < PciCount; PciIndex++) {
    UINT16  Vendor;
    UINT16  Device;

    Status = gBS->HandleProtocol (
                    PciHandles[PciIndex],
                    &gEfiPciIoProtocolGuid,
                    (VOID *)&gPciIo
                    );

    if (EFI_ERROR (Status)) {
      Print (L"Couldn't get PciIo: %r\n", Status);
      continue;
    }

    Status = gPciIo->Pci.Read (
                           gPciIo,
                           EfiPciIoWidthUint16,
                           PCI_VENDOR_ID_OFFSET,
                           1,
                           &Vendor
                           );
    if (EFI_ERROR (Status)) {
      Print (L"Couldn't read offset %u: %r\n", PCI_VENDOR_ID_OFFSET, Status);
      continue;
    }

    Status = gPciIo->Pci.Read (
                           gPciIo,
                           EfiPciIoWidthUint16,
                           PCI_DEVICE_ID_OFFSET,
                           1,
                           &Device
                           );
    if (EFI_ERROR (Status)) {
      Print (L"Couldn't read offset %u: %r\n", PCI_DEVICE_ID_OFFSET, Status);
      continue;
    }

    Print (L"Examining 0x%04x:0x%04x\n", Vendor, Device);

    if ((Vendor != 0x10ee) || (Device != 0x0666)) {
      continue;
    }

    break;
  }

  if (PciIndex == PciCount) {
    Print (L"PCIe Screamer not found out of %u devices\n", PciCount);
    Status = EFI_NOT_FOUND;
    goto out;
  }

  Print (L"PCIe Screamer found\n");

  SacPrint ("Hello SAC console from UEFI!\n");

out:
  gBS->FreePool (PciHandles);
  return Status;
}

/** @file
  SAC device support.

  Copyright (C) 2023 Andrei Warkentin. All rights reserved.<BR>
  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SacDxe.h"

#define SIO_TIMEOUT_STALL_INTERVAL  10

EFI_STATUS
EFIAPI
SacSerialReset (
  IN EFI_SERIAL_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

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
  )
{
  This->Mode->BaudRate         = BaudRate;
  This->Mode->ReceiveFifoDepth = ReceiveFifoDepth;
  This->Mode->Timeout          = Timeout;
  This->Mode->Parity           = Parity;
  This->Mode->DataBits         = DataBits;
  This->Mode->StopBits         = StopBits;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SacSerialSetControl (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN UINT32                  Control
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
SacSerialGetControl (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  OUT UINT32                 *Control
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
SacSerialWrite (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN OUT UINTN               *BufferSize,
  IN VOID                    *Buffer
  )
{
  UINT8             *Char;
  UINTN             SizeLeft;
  SAC_PRIVATE_DATA  *Private;

  if (*BufferSize == 0) {
    return EFI_SUCCESS;
  }

  if (Buffer == NULL) {
    return EFI_DEVICE_ERROR;
  }

  Private  = SAC_PRIVATE_FROM_SIO (This);
  SizeLeft = *BufferSize;
  Char     = Buffer;

  while (SizeLeft-- != 0) {
    SacConWrite (Private, *Char++);
  }

  *BufferSize = (UINTN)Char - (UINTN)Buffer;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SacSerialRead (
  IN EFI_SERIAL_IO_PROTOCOL  *This,
  IN OUT UINTN               *BufferSize,
  OUT VOID                   *Buffer
  )
{
  UINTN             Count;
  UINTN             TimeOut;
  UINT8             *Data;
  SAC_PRIVATE_DATA  *Private;

  Count   = 0;
  Data    = Buffer;
  Private = SAC_PRIVATE_FROM_SIO (This);
  while (Count < *BufferSize) {
    TimeOut = 0;
    while (TimeOut < This->Mode->Timeout) {
      if (SacConPoll (Private)) {
        break;
      }

      gBS->Stall (SIO_TIMEOUT_STALL_INTERVAL);
      TimeOut += SIO_TIMEOUT_STALL_INTERVAL;
    }

    if (TimeOut >= This->Mode->Timeout) {
      break;
    }

    *Data = (UINT8)SacConRead (Private, FALSE);
    Count++;
    Data++;
  }

  if (Count != *BufferSize) {
    *BufferSize = Count;
    return EFI_TIMEOUT;
  }

  return EFI_SUCCESS;
}

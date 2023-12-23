#pragma once

#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/DevicePath.h>
#include <Protocol/PciIo.h>
#include <Protocol/ComponentName.h>
#include <Protocol/DriverBinding.h>
#include <IndustryStandard/Pci.h>

#define SAC_CFG_TEXT_OUT 0x200
#define SAC_CFG_TEXT_IN  0x200

#define SAC_SIGNATURE  SIGNATURE_32('S','A','C','D')

extern EFI_COMPONENT_NAME_PROTOCOL   gSacComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL  gSacComponentName2;
extern EFI_DRIVER_BINDING_PROTOCOL  gSacDriverBinding;

#define SAC_PRIVATE_FROM_STO(a) CR(a, SAC_PRIVATE_DATA, SimpleTextOut, SAC_SIGNATURE)
#define SAC_PRIVATE_FROM_STI(a) CR(a, SAC_PRIVATE_DATA, SimpleTextIn, SAC_SIGNATURE)

typedef struct {
  UINT32 Signature;
  EFI_PCI_IO_PROTOCOL *PciIo;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL SimpleTextOut;
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL SimpleTextIn;
  EFI_SIMPLE_TEXT_OUTPUT_MODE SimpleTextOutMode;
  UINT8 ReadData;
} SAC_PRIVATE_DATA;

EFI_STATUS
EFIAPI
TextOutReset (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN BOOLEAN                          ExtendedVerification
  );

EFI_STATUS
EFIAPI
OutputString (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN CHAR16                           *String
  );

EFI_STATUS
EFIAPI
TestString (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN CHAR16                           *String
  );

EFI_STATUS
EFIAPI
QueryMode (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN UINTN                            ModeNumber,
  OUT UINTN                           *Columns,
  OUT UINTN                           *Rows
  );

EFI_STATUS
EFIAPI
SetMode (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN UINTN                            ModeNumber
  );

EFI_STATUS
EFIAPI
SetAttribute (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN UINTN                            Attribute
  );

EFI_STATUS
EFIAPI
ClearScreen (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This
  );

EFI_STATUS
EFIAPI
SetCursorPosition (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN UINTN                            Column,
  IN UINTN                            Row
  );

EFI_STATUS
EFIAPI
EnableCursor (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN BOOLEAN                          Enable
  );

EFI_STATUS
EFIAPI
TextInReset (
  IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *This,
  IN BOOLEAN                         ExtendedVerification
  );

EFI_STATUS
EFIAPI
ReadKeyStroke (
  IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *This,
  OUT EFI_INPUT_KEY                  *Key
  );

VOID
EFIAPI
WaitForKeyEvent (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  );

VOID
SacConWrite (IN  SAC_PRIVATE_DATA *Private,
             IN  UINT8 Data);

UINT8
SacConRead (IN  SAC_PRIVATE_DATA *Private,
            IN  BOOLEAN WaitForData);

BOOLEAN
SacConPoll (IN  SAC_PRIVATE_DATA *Private);

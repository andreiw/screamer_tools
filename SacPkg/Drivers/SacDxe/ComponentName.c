/** @file
  SAC device support.

  Copyright (C) 2023 Andrei Warkentin. All rights reserved.<BR>
  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "SacDxe.h"

EFI_STATUS
EFIAPI
SacComponentNameGetDriverName (
  IN EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN CHAR8                        *Language,
  OUT CHAR16                      **DriverName
  );

EFI_STATUS
EFIAPI
SacComponentNameGetControllerName (
  IN EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_HANDLE                   ChildHandle OPTIONAL,
  IN CHAR8                        *Language,
  OUT CHAR16                      **ControllerName
  );

EFI_COMPONENT_NAME_PROTOCOL  gSacComponentName = {
  SacComponentNameGetDriverName,
  SacComponentNameGetControllerName,
  "eng"
};

EFI_COMPONENT_NAME2_PROTOCOL  gSacComponentName2 = {
  (EFI_COMPONENT_NAME2_GET_DRIVER_NAME)SacComponentNameGetDriverName,
  (EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME)SacComponentNameGetControllerName,
  "en"
};

EFI_UNICODE_STRING_TABLE  mSacDriverNameTable[] = {
  {
    "eng;en",
    (CHAR16 *)L"SAC Driver"
  },
  {
    NULL,
    NULL
  }
};

EFI_UNICODE_STRING_TABLE  mSacControllerNameTable[] = {
  {
    "eng;en",
    (CHAR16 *)L"SAC Adapter"
  },
  {
    NULL,
    NULL
  }
};

EFI_STATUS
EFIAPI
SacComponentNameGetDriverName (
  IN EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN CHAR8                        *Language,
  OUT CHAR16                      **DriverName
  )
{
  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           mSacDriverNameTable,
           DriverName,
           (BOOLEAN)(This == &gSacComponentName)
           );
}

EFI_STATUS
EFIAPI
SacComponentNameGetControllerName (
  IN EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_HANDLE                   ChildHandle OPTIONAL,
  IN CHAR8                        *Language,
  OUT CHAR16                      **ControllerName
  )
{
  EFI_STATUS  Status;

  Status = EfiTestManagedDevice (
             ControllerHandle,
             gSacDriverBinding.DriverBindingHandle,
             &gEfiPciIoProtocolGuid
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (ChildHandle != NULL) {
    return EFI_UNSUPPORTED;
  }

  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           mSacControllerNameTable,
           ControllerName,
           (BOOLEAN)(This == &gSacComponentName)
           );
}

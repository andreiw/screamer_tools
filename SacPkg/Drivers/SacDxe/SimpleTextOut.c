#include "SacDxe.h"

#define MODE0_COLUMN_COUNT  80
#define MODE0_ROW_COUNT     25

STATIC
BOOLEAN
TextOutIsValidAscii (
  IN CHAR16  Ascii
  )
{
  //
  // valid ASCII code lies in the extent of 0x20 - 0x7F
  //
  if ((Ascii >= 0x20) && (Ascii <= 0x7F)) {
    return TRUE;
  }

  return FALSE;
}

STATIC
BOOLEAN
TextOutIsValidEfiCntlChar (
  IN CHAR16  Char
  )
{
  //
  // only support four control characters.
  //
  if ((Char == CHAR_NULL) ||
      (Char == CHAR_BACKSPACE) ||
      (Char == CHAR_LINEFEED) ||
      (Char == CHAR_CARRIAGE_RETURN) ||
      (Char == CHAR_TAB))
  {
    return TRUE;
  }

  return FALSE;
}

EFI_STATUS
EFIAPI
TextOutReset (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN BOOLEAN                          ExtendedVerification
  )
{
  EFI_STATUS  Status;

  This->SetAttribute (
          This,
          EFI_TEXT_ATTR (This->Mode->Attribute & 0x0F, EFI_BACKGROUND_BLACK)
          );

  Status = This->SetMode (This, 0);

  return Status;
}

STATIC
VOID
SafeUnicodeWrite (
  IN      SAC_PRIVATE_DATA *Private,
  IN      CONST CHAR16  *Source
  )
{
  //
  // ASSERT if Source is long than PcdMaximumUnicodeStringLength.
  // Length tests are performed inside StrLen().
  //
  ASSERT (StrSize (Source) != 0);

  while (*Source != '\0') {
    //
    // If any non-ascii characters in Source then replace it with '?'.
    //
    if (*Source < 0x80) {
      SacConWrite (Private, (CHAR8)*Source);
    } else {
      SacConWrite (Private, '?');

      // Surrogate pair check.
      if ((*Source >= 0xD800) && (*Source <= 0xDFFF)) {
        Source++;
      }
    }

    Source++;
  }
}

EFI_STATUS
EFIAPI
OutputString (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN CHAR16                           *String
  )
{
  EFI_STATUS                   Status;
  EFI_SIMPLE_TEXT_OUTPUT_MODE  *Mode;
  UINTN                        MaxColumn;
  UINTN                        MaxRow;
  SAC_PRIVATE_DATA *Private;

  Private = SAC_PRIVATE_FROM_STO (This);

  // If there is any non-ascii characters in String buffer then replace it with '?'
  SafeUnicodeWrite (Private, String);

  //
  // Parse each character of the string to output
  // to update the cursor position information
  //
  Mode = This->Mode;

  Status = This->QueryMode (
                   This,
                   Mode->Mode,
                   &MaxColumn,
                   &MaxRow
                   );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for ( ; *String != CHAR_NULL; String++) {
    switch (*String) {
      case CHAR_BACKSPACE:
        if (Mode->CursorColumn > 0) {
          Mode->CursorColumn--;
        }

        break;

      case CHAR_LINEFEED:
        if (Mode->CursorRow < (INT32)(MaxRow - 1)) {
          Mode->CursorRow++;
        }

        break;

      case CHAR_CARRIAGE_RETURN:
        Mode->CursorColumn = 0;
        break;

      default:
        if (Mode->CursorColumn >= (INT32)(MaxColumn - 1)) {
          // Move the cursor as if we print CHAR_CARRIAGE_RETURN & CHAR_LINE_FEED
          // CHAR_LINEFEED
          if (Mode->CursorRow < (INT32)(MaxRow - 1)) {
            Mode->CursorRow++;
          }

          // CHAR_CARIAGE_RETURN
          Mode->CursorColumn = 0;
        } else {
          Mode->CursorColumn++;
        }

        break;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
TestString (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN CHAR16                           *String
  )
{
  CHAR8  Character;

  for ( ; *String != CHAR_NULL; String++) {
    Character = (CHAR8)*String;
    if (!(TextOutIsValidAscii (Character) || TextOutIsValidEfiCntlChar (Character))) {
      return EFI_UNSUPPORTED;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
QueryMode (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN UINTN                            ModeNumber,
  OUT UINTN                           *Columns,
  OUT UINTN                           *Rows
  )
{
  if (This->Mode->MaxMode > 1) {
    return EFI_DEVICE_ERROR;
  }

  if (ModeNumber == 0) {
    *Columns = MODE0_COLUMN_COUNT;
    *Rows    = MODE0_ROW_COUNT;
    return EFI_SUCCESS;
  }

  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
SetMode (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN UINTN                            ModeNumber
  )
{
  if (ModeNumber != 0) {
    return EFI_UNSUPPORTED;
  }

  This->Mode->Mode = 0;
  This->ClearScreen (This);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SetAttribute (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN UINTN                            Attribute
  )
{
  This->Mode->Attribute = (INT32)Attribute;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ClearScreen (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This
  )
{
  EFI_STATUS  Status;

  Status = This->SetCursorPosition (This, 0, 0);
  return Status;
}

EFI_STATUS
EFIAPI
SetCursorPosition (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN UINTN                            Column,
  IN UINTN                            Row
  )
{
  EFI_SIMPLE_TEXT_OUTPUT_MODE  *Mode;
  EFI_STATUS                   Status;
  UINTN                        MaxColumn;
  UINTN                        MaxRow;

  Mode = This->Mode;

  Status = This->QueryMode (
                   This,
                   Mode->Mode,
                   &MaxColumn,
                   &MaxRow
                   );
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  if ((Column >= MaxColumn) || (Row >= MaxRow)) {
    return EFI_UNSUPPORTED;
  }

  Mode->CursorColumn = (INT32)Column;
  Mode->CursorRow    = (INT32)Row;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
EnableCursor (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN BOOLEAN                          Enable
  )
{
  if (!Enable) {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

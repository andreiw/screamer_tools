#include "SacDxe.h"

/*
 * Symbols used in table below
 * ===========================
 *   ESC = 0x1B
 *   CSI = 0x9B
 *   DEL = 0x7f
 *   ^   = CTRL
 *
 * +=========+======+===========+==========+==========+
 * |         | EFI  | UEFI 2.0  |          |          |
 * |         | Scan |           |  VT100+  |          |
 * |   KEY   | Code |  PC ANSI  |  VTUTF8  |   VT100  |
 * +=========+======+===========+==========+==========+
 * | NULL    | 0x00 |           |          |          |
 * | UP      | 0x01 | ESC [ A   | ESC [ A  | ESC [ A  |
 * | DOWN    | 0x02 | ESC [ B   | ESC [ B  | ESC [ B  |
 * | RIGHT   | 0x03 | ESC [ C   | ESC [ C  | ESC [ C  |
 * | LEFT    | 0x04 | ESC [ D   | ESC [ D  | ESC [ D  |
 * | HOME    | 0x05 | ESC [ H   | ESC h    | ESC [ H  |
 * | END     | 0x06 | ESC [ F   | ESC k    | ESC [ K  |
 * | INSERT  | 0x07 | ESC [ @   | ESC +    | ESC [ @  |
 * |         |      | ESC [ L   |          | ESC [ L  |
 * | DELETE  | 0x08 | ESC [ X   | ESC -    | ESC [ P  |
 * | PG UP   | 0x09 | ESC [ I   | ESC ?    | ESC [ V  |
 * |         |      |           |          | ESC [ ?  |
 * | PG DOWN | 0x0A | ESC [ G   | ESC /    | ESC [ U  |
 * |         |      |           |          | ESC [ /  |
 * | F1      | 0x0B | ESC [ M   | ESC 1    | ESC O P  |
 * | F2      | 0x0C | ESC [ N   | ESC 2    | ESC O Q  |
 * | F3      | 0x0D | ESC [ O   | ESC 3    | ESC O w  |
 * | F4      | 0x0E | ESC [ P   | ESC 4    | ESC O x  |
 * | F5      | 0x0F | ESC [ Q   | ESC 5    | ESC O t  |
 * | F6      | 0x10 | ESC [ R   | ESC 6    | ESC O u  |
 * | F7      | 0x11 | ESC [ S   | ESC 7    | ESC O q  |
 * | F8      | 0x12 | ESC [ T   | ESC 8    | ESC O r  |
 * | F9      | 0x13 | ESC [ U   | ESC 9    | ESC O p  |
 * | F10     | 0x14 | ESC [ V   | ESC 0    | ESC O M  |
 * | Escape  | 0x17 | ESC       | ESC      | ESC      |
 * | F11     | 0x15 |           | ESC !    |          |
 * | F12     | 0x16 |           | ESC @    |          |
 * +=========+======+===========+==========+==========+
 */

EFI_STATUS
EFIAPI
TextInReset (
  IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *This,
  IN BOOLEAN                         ExtendedVerification
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ReadKeyStroke (
  IN EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *This,
  OUT EFI_INPUT_KEY                  *Key
  )
{
  UINT8  Char;
  SAC_PRIVATE_DATA *Private;

  Private = SAC_PRIVATE_FROM_STI (This);

  Char = SacConRead (Private, FALSE);
  if (Char == 0 || Char == 0xFF) {
    return EFI_NOT_READY;
  }

  //
  // Check for ESC sequence. This code is not technically correct VT100 code.
  // An illegal ESC sequence represents an ESC and the characters that follow.
  // This code will eat one or two chars after an escape. This is done to
  // prevent some complex FIFOing of the data. It is good enough to get
  // the arrow and delete keys working
  //
  Key->UnicodeChar = 0;
  Key->ScanCode    = SCAN_NULL;
  if (Char == 0x1b) {
    Char = SacConRead (Private, TRUE);
    if (Char == '[') {
      Char = SacConRead (Private, TRUE);
      switch (Char) {
        case 'A':
          Key->ScanCode = SCAN_UP;
          break;
        case 'B':
          Key->ScanCode = SCAN_DOWN;
          break;
        case 'C':
          Key->ScanCode = SCAN_RIGHT;
          break;
        case 'D':
          Key->ScanCode = SCAN_LEFT;
          break;
        case 'H':
          Key->ScanCode = SCAN_HOME;
          break;
        case 'K':
        case 'F': // PC ANSI
          Key->ScanCode = SCAN_END;
          break;
        case '@':
        case 'L':
          Key->ScanCode = SCAN_INSERT;
          break;
        case 'P':
        case 'X': // PC ANSI
          Key->ScanCode = SCAN_DELETE;
          break;
        case 'U':
        case '/':
        case 'G': // PC ANSI
          Key->ScanCode = SCAN_PAGE_DOWN;
          break;
        case 'V':
        case '?':
        case 'I': // PC ANSI
          Key->ScanCode = SCAN_PAGE_UP;
          break;

        // PCANSI that does not conflict with VT100
        case 'M':
          Key->ScanCode = SCAN_F1;
          break;
        case 'N':
          Key->ScanCode = SCAN_F2;
          break;
        case 'O':
          Key->ScanCode = SCAN_F3;
          break;
        case 'Q':
          Key->ScanCode = SCAN_F5;
          break;
        case 'R':
          Key->ScanCode = SCAN_F6;
          break;
        case 'S':
          Key->ScanCode = SCAN_F7;
          break;
        case 'T':
          Key->ScanCode = SCAN_F8;
          break;

        default:
          Key->UnicodeChar = Char;
          break;
      }
    } else if (Char == '0') {
      Char = SacConRead (Private, TRUE);
      switch (Char) {
        case 'P':
          Key->ScanCode = SCAN_F1;
          break;
        case 'Q':
          Key->ScanCode = SCAN_F2;
          break;
        case 'w':
          Key->ScanCode = SCAN_F3;
          break;
        case 'x':
          Key->ScanCode = SCAN_F4;
          break;
        case 't':
          Key->ScanCode = SCAN_F5;
          break;
        case 'u':
          Key->ScanCode = SCAN_F6;
          break;
        case 'q':
          Key->ScanCode = SCAN_F7;
          break;
        case 'r':
          Key->ScanCode = SCAN_F8;
          break;
        case 'p':
          Key->ScanCode = SCAN_F9;
          break;
        case 'm':
          Key->ScanCode = SCAN_F10;
          break;
        default:
          break;
      }
    }
  } else if (Char < ' ') {
    if ((Char == CHAR_BACKSPACE) ||
        (Char == CHAR_TAB)       ||
        (Char == CHAR_LINEFEED)  ||
        (Char == CHAR_CARRIAGE_RETURN))
    {
      // Only let through EFI required control characters
      Key->UnicodeChar = (CHAR16)Char;
    }
  } else if (Char == 0x7f) {
    Key->ScanCode = SCAN_DELETE;
  } else {
    Key->UnicodeChar = (CHAR16)Char;
  }

  return EFI_SUCCESS;
}

VOID
EFIAPI
WaitForKeyEvent (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  SAC_PRIVATE_DATA *Private;

  Private = Context;

  if (SacConPoll (Private)) {
    gBS->SignalEvent (Event);
  }
}

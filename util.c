#include "screamer.h"

void
hex_dump(uint8_t *buffer,
         int num_bytes,
         int line_length)
{
  int i;
  int offset = 0;

  while (num_bytes != 0) {
    int now = num_bytes;
    if (now > line_length) {
      now = line_length;
    }

    printf ("%6X |", offset);
    for (i = 0; i < now; i++) {
      if (i > 0 && i % 4 == 0) {
        putchar (' ');
      }
      printf (" %02X", buffer[i]);
    }

    for (; i < line_length; i++) {
      if (i > 0 && i % 4 == 0) {
        putchar (' ');
      }
      printf ("   ");
    }

    printf (" | ");

    for (i = 0; i < now; i++) {
      if (isalnum (buffer[i])) {
        putchar (buffer[i]);
      } else {
        putchar ('.');
      }
    }

    num_bytes -= now;
    offset += now;
    buffer += now;
    putchar ('\n');
  }
}

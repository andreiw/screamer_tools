/*
 * Part of screamer_tools.
 *
 * SPDX-License-Identifier: GPL-3.0
 */

#include "screamer.h"

static int socket_fd = -1;
static struct sockaddr_in sa;

int
net_dump_init (char *remote_addr,
               in_port_t remote_port)
{
  socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_fd < 0) {
    fprintf(stderr, "socket: %s\n", strerror (errno));
    return -1;
  }

  sa.sin_family = AF_INET;
  sa.sin_port = htons(remote_port);
  sa.sin_addr.s_addr = inet_addr(remote_addr);
  return 0;
}

void
net_dump(void *buffer,
         int num_bytes)
{
  int err;

  if (socket_fd == -1) {
    return;
  }

  err = sendto(socket_fd, buffer, num_bytes,
               0, (struct sockaddr *) &sa,
               sizeof (sa));
  if (err < 0)  {
    fprintf (stderr, "sendto: %s\n", strerror (errno));
  }
}

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

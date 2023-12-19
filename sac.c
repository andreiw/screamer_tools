/*
 * A character device that monitors config register 0x200,
 * outputing everything that is sent over.
 *
 * Requires the pcileech gateware.
 *
 * SPDX-License-Identifier: GPL-3.0
 */

#include "screamer.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>

static bool verbose;
static struct termios termios_orig;

static int
parse_opts (int argc,
            char **argv,
            unsigned long *device_index)
{
  int opt;

  while ((opt = getopt(argc, argv, "n:v")) != -1) {
    switch (opt) {
    case 'n':
      *device_index = strtoul (optarg, NULL, 10);
      break;
    case 'v':
      verbose = true;
      break;
    default: /* '?' */
      fprintf(stderr, "Usage: %s [-n device_index] [-p port] [-v] [remote server]\n",
              argv[0]);
      return -1;
    }
  }

  return 0;
}

static void
term_restore (void)
{
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &termios_orig);
}

static void
term_raw (void)
{
  struct termios raw;

  setbuf (stdout, 0);
  setbuf (stderr, 0);

  if (tcgetattr(STDIN_FILENO, &termios_orig) == -1) {
    fprintf (stderr, "Couldn't tcgetattr: %s\r\n", strerror (errno));
  } else {
    raw = termios_orig;
    atexit(term_restore);

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    /*
     * Setting ISIG makes things awkward. UEFI doesn't really use Ctrl sequences.
     */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
      fprintf (stderr, "Couldn't tcsetattr: %s\r\n", strerror (errno));
    }
  }

  if (fcntl(STDIN_FILENO, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK) < 0) {
    fprintf (stderr, "Couldn't make stdin non-blocking: %s\r\n", strerror (errno));
  }
}

int
main (int argc,
      char **argv)
{
  int err;
  unsigned long device_index;
  tlp_process_context context;

  device_index = 0;
  err = parse_opts (argc, argv, &device_index);
  if (err != 0) {
    return -1;
  };

  err = ftdi_get (device_index);
  if (err != 0) {
    fprintf (stderr, "No FTDI device found\n");
    return -1;
  }

  err = fpga_init ();
  if (err != 0) {
    fprintf (stderr, "FPGA init failed\n");
    return -1;
  }

  term_raw ();

  memset (&context, 0, sizeof (context));
  while (1) {
    tlp_t tlp;
    void *tlp_data;
    uint8_t *payload;
    uint32_t tlp_size;
    tlp_process_result_t state;
    int payload_len_dws = 0;

    state = fpga_process_tlp (&context, &tlp_data, &tlp_size);
    if (state != TLP_COMPLETE) {
      continue;
    }

    payload = tlp_packet_to_host (tlp_data, &tlp,
                                  &payload_len_dws);
    if (tlp.header._fmt_type != TLP_CfgWr0) {
      continue;
    }
    if (tlp.cfg.last_be != 0 ||
        (tlp.cfg.first_be & 0x1) != 0x1) {
      continue;
    }

    if (tlp_cfg_reg (&tlp.cfg) != 0x200) {
      continue;
    }

    if (payload_len_dws != 1) {
      continue;
    }

    putchar (*payload);
  }

  return 0;
}

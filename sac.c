/*
 * A character device that monitors config register 0x200,
 * outputing everything that is sent over.
 *
 * Requires the pcileech gateware.
 *
 * SPDX-License-Identifier: GPL-3.0
 */

#include "screamer.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>

static bool verbose;
static bool remote_dump;
static struct termios termios_orig;

static int
parse_opts (int argc,
            char **argv,
            unsigned long *device_index,
            char **remote_ip,
            in_port_t *remote_port)
{
  int opt;

  while ((opt = getopt(argc, argv, "dn:v")) != -1) {
    switch (opt) {
    case 'n':
      *device_index = strtoul (optarg, NULL, 10);
      break;
    case 'v':
      verbose = true;
      break;
    case 'd':
      remote_dump = true;
      break;
    default: /* '?' */
      fprintf(stderr, "Usage: %s [-n device_index] [-v] [-d [remote server] [port]]\n",
              argv[0]);
      return -1;
    }
  }

  if (remote_dump) {
    if (optind < argc) {
      *remote_ip = argv[optind++];
    }
    if (optind < argc) {
      *remote_port = (in_port_t) strtoul (argv[optind],
                                          NULL, 10);
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
  tlp_receive_context context;
  char *remote_addr;
  in_port_t remote_port;

  device_index = 0;
  remote_addr = "127.0.0.1";
  remote_port = 9999;
  err = parse_opts (argc, argv, &device_index,
                    &remote_addr, &remote_port);
  if (err != 0) {
    return -1;
  };

  if (remote_dump) {
    err = net_dump_init (remote_addr, remote_port);
    if (err < 0) {
      fprintf(stderr, "net_dump_init: %s\n", strerror (errno));
      return -1;
    }
  }

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
    tlp_t cpl_tlp;
    void *rx_tlp_data;
    uint32_t rx_tlp_size;
    uint8_t tx_tlp_data[sizeof (tlp_cpl_t) + sizeof(uint32_t)];
    uint32_t tx_tlp_size;
    uint32_t *payload;
    tlp_receive_result_t state;
    int payload_len_dws = 0;

    state = fpga_tlp_receive (&context, &rx_tlp_data,
                              &rx_tlp_size);
    if (state != TLP_COMPLETE) {
      if (state == TLP_CORRUPT) {
        if (verbose) {
          fprintf (stderr, "Corrupt TLP received\n");
        }
        net_dump (rx_tlp_data, rx_tlp_size);
      } else if (state == TLP_OUT_OF_SYNC) {
        if (verbose) {
          fprintf (stderr, "FPGA out of sync\n");
        }
      }
      continue;
    }

    payload = tlp_packet_to_host (rx_tlp_data, &tlp,
                                  &payload_len_dws);

    memset (&cpl_tlp, 0, sizeof (cpl_tlp.cpl));
    cpl_tlp.hdr.attr = tlp.hdr.attr;
    cpl_tlp.hdr.tc = tlp.hdr.tc;
    cpl_tlp.hdr._fmt_type = TLP_Cpl;

    if ((tlp.hdr._fmt_type == TLP_CfgWr0 ||
         tlp.hdr._fmt_type == TLP_CfgRd0)) {
      cpl_tlp.cpl._cid = tlp.cfg._cid;
      cpl_tlp.cpl.tag = tlp.cfg.tag;
      cpl_tlp.cpl._rid = tlp.cfg._rid;
      cpl_tlp.cpl.byte_count = 4;
      tx_tlp_size = sizeof (tlp_cpl_t);

      if (tlp.cfg.last_be != 0 ||
          (tlp.cfg.first_be & 0x1) != 1 ||
          tlp_cfg_reg (&tlp.cfg) != 0x200) {
        cpl_tlp.cpl.status = TLP_CPL_STATUS_UR;

        tlp_host_to_packet (&cpl_tlp, &tx_tlp_data,
                            tx_tlp_size);
      } else {
        if (tlp.hdr._fmt_type == TLP_CfgWr0 &&
            payload_len_dws == 1) {

          putchar ((uint8_t) *payload);

          tlp_host_to_packet (&cpl_tlp, &tx_tlp_data,
                              tx_tlp_size);
        } else if (tlp.hdr._fmt_type == TLP_CfgRd0 &&
                   payload_len_dws == 0) {

          cpl_tlp.hdr._fmt_type = TLP_CplD;
          cpl_tlp.hdr.length = 1;

          tx_tlp_size += 4;
          payload = tlp_host_to_packet (&cpl_tlp, &tx_tlp_data,
                                        tx_tlp_size);
          *payload = 0xffffff00 | getchar();
        }
      }

      if (fpga_tlp_send (tx_tlp_data, tx_tlp_size) != 0) {
        fprintf (stderr, "Failed to send completion\n");
      }
    }
  }
}

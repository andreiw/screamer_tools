/*
 * Dumps incoming TLPs to console (or to remote UDP
 * socket). Also see wireshark/pcietlp.lua.
 *
 * Requires the pcileech gateware.
 *
 * SPDX-License-Identifier: GPL-3.0
 */

#include "screamer.h"

static bool verbose;

static int
parse_opts(int argc,
           char **argv,
           unsigned long *device_index,
           char **remote_ip,
           in_port_t *remote_port)
{
  int opt;

  while ((opt = getopt(argc, argv, "n:p:v")) != -1) {
    switch (opt) {
    case 'n':
      *device_index = strtoul (optarg, NULL, 10);
      break;
    case 'p':
      *remote_port = (in_port_t) strtoul (optarg, NULL, 10);
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

  if (optind < argc) {
    *remote_ip = argv[optind];
  }

  return 0;
}

int
main (int argc,
      char **argv)
{
  int err;
  unsigned long device_index;
  char *remote_addr;
  in_port_t remote_port;
  tlp_receive_context context;

  device_index = 0;
  remote_addr = "127.0.0.1";
  remote_port = 9999;
  err = parse_opts (argc, argv, &device_index,
                    &remote_addr, &remote_port);
  if (err != 0) {
    return -1;
  };

  printf ("UDP server is %s:%u\n", remote_addr, remote_port);
  err = net_dump_init (remote_addr, remote_port);
  if (err < 0) {
    fprintf(stderr, "net_dump_init: %s\n", strerror (errno));
    return -1;
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

  memset (&context, 0, sizeof (context));
  while (1) {
    void *tlp_data;
    uint32_t tlp_size;
    tlp_receive_result_t state;

    state = fpga_tlp_receive (&context, &tlp_data, &tlp_size);
    if (state == TLP_OUT_OF_SYNC) {
      fprintf (stderr, "Missing header\n");
    } else if (state == TLP_CORRUPT) {
      fprintf (stderr, "Bad PCIe TLP received\n");
    } else if (state == TLP_COMPLETE) {
      if (verbose) {
        printf ("TLP of 0x%x bytes\n", tlp_size);
        hex_dump (tlp_data, tlp_size, 16);
      }

      net_dump (tlp_data, tlp_size);
      if (err < 0)  {
        fprintf (stderr, "sendto: %s\n", strerror (errno));
      }
    }
  }

  return 0;
}

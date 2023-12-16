/*
 * Requires the pcileech gateware.
 *
 * Based on:
 * - https://github.com/ufrisk/LeechCore-plugins/tree/master/leechcore_ft601_driver_linux (GPLv3)
 * - https://github.com/ufrisk/LeechCore/blob/master/leechcore/device_fpga.c (GPLv3)
 *
 * SPDX-License-Identifier: GPL-3.0
 *
 * TBD:
 * - not endian safe (assumes LE)
 * - sketchy buffer handling in fpga_config_read
 *   (0x400 should be enough, or just reuse the
 *   global data buffer).
 * - split up into ftdi/fpga/main components.
 * - BAR enable.
 */

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <byteswap.h>
#include <libusb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include "ft60x.h"

#define FPGA_CMD_VERSION_MAJOR          0x01
#define FPGA_CMD_DEVICE_ID              0x03
#define FPGA_CMD_VERSION_MINOR          0x05

#define FPGA_REG_CORE                 0x0003
#define FPGA_REG_PCIE                 0x0001
#define FPGA_REG_READONLY             0x0000
#define FPGA_REG_READWRITE            0x8000
#define FPGA_REG_SHADOWCFGSPACE       0xC000

#define MS_TO_US(x) ((x) * 1000)

static bool verbose;
static libusb_context *usb_ctx;
static libusb_device_handle *device_handle;

#define TLP_RX_MAX_SIZE             (16+1024)
#define TLP_RX_MAX_SIZE_IN_DWORDS   ((int) (TLP_RX_MAX_SIZE / sizeof (uint32_t)))

static uint8_t data[TLP_RX_MAX_SIZE];
static uint32_t tlp_dwords[TLP_RX_MAX_SIZE_IN_DWORDS];

static int ftdi_set_config (ft60x_config *config)
{
  return libusb_control_transfer (device_handle,
                                  LIBUSB_RECIPIENT_DEVICE |
                                  LIBUSB_REQUEST_TYPE_VENDOR |
                                  LIBUSB_ENDPOINT_OUT,
                                  0xCF,
                                  0,
                                  0,
                                  (void *) config,
                                  sizeof (*config),
                                  1000
                                  );
}

static int ftdi_get_config (ft60x_config *config)
{
  return libusb_control_transfer(device_handle,
                                 LIBUSB_RECIPIENT_DEVICE |
                                 LIBUSB_REQUEST_TYPE_VENDOR |
                                 LIBUSB_ENDPOINT_IN,
                                 0xCF,
                                 1,
                                 0,
                                 (void *) config,
                                 sizeof (*config),
                                 1000
                                 );
}

static int ftdi_send_cmd_read (int size)
{
  int transferred;
  ft60x_ctrl_req ctrl_req;

  memset (&ctrl_req, 0, sizeof (ctrl_req));
  ctrl_req.idx++;
  ctrl_req.pipe = FTDI_ENDPOINT_IN;
  ctrl_req.cmd = 1;
  ctrl_req.len = size;
  transferred = 0;

  return libusb_bulk_transfer (device_handle, FTDI_ENDPOINT_SESSION_OUT,
                               (void *) &ctrl_req, sizeof (ctrl_req),
                               &transferred, 1000);
}

static int ftdi_write (void *data, int size,
                       int *transferred)
{
  int err;

  *transferred = 0;
  err = libusb_bulk_transfer (device_handle, FTDI_ENDPOINT_OUT,
                              data, size, transferred, 1000);
  if (err < 0) {
    fprintf (stderr, "libusb_bulk_transfer: %s\n",
             libusb_strerror (err));
    return -1;

    if (*transferred != size) {
      fprintf (stderr, "only %d/%d bytes transferred\n",
               *transferred, size);
      return -1;
    }
  }

  return 0;
}

static int ftdi_read (void *data, int size,
                      int *transferred)
{
  int err;

  err = ftdi_send_cmd_read (size);
  if (err != 0) {
    fprintf (stderr, "ftdi_send_cmd_read: %s\n", libusb_strerror(err));
    return -1;
  }

  *transferred = 0;
  err = libusb_bulk_transfer(device_handle, FTDI_ENDPOINT_IN, data, size, transferred, 0);
  if (err < 0) {
    fprintf (stderr, "libusb_bulk_transfer: %s\n", libusb_strerror(err));
    return -1;
  }

  return 0;
}

static int fpga_config_write (uint16_t address,
                              void *data,
                              uint16_t count,
                              uint16_t flags)
{
  int err;
  uint8_t buf[0x800];
  uint16_t cur_addr;
  int buf_index;
  int i;

  if (count == 0 || (address + count) > 0x1000) {
    return -1;
  }

  i = 0;
  buf_index = 0;
  if ((address % 2) != 0) {
    /*
     * Byte align if required.
     */
    cur_addr = (address - 1) | (flags & 0xC000);
    buf[buf_index + 0] = 0x00;
    buf[buf_index + 1] = *((uint8_t *) data);
    buf[buf_index + 2] = 0x00;
    buf[buf_index + 3] = 0xff;
    buf[buf_index + 4] = cur_addr >> 8;
    buf[buf_index + 5] = cur_addr & 0xff;
    buf[buf_index + 6] = 0x20 | (flags & 0x03);
    buf[buf_index + 7] = 0x77;
    buf_index += 8;
    i++;
  }

  for (; i < count; i += 2) {
    cur_addr = (address + i) | (flags & 0xC000);
    buf[buf_index + 0] = ((uint8_t *) data)[i];
    buf[buf_index + 1] = (count == i + 1) ? 0 : ((uint8_t *) data)[i + 1];
    buf[buf_index + 2] = 0xff;
    buf[buf_index + 3] = (count == i + 1) ? 0 : 0xff;
    buf[buf_index + 4] = cur_addr >> 8;
    buf[buf_index + 5] = cur_addr & 0xff;
    buf[buf_index + 6] = 0x20 | (flags & 0x03);
    buf[buf_index + 7] = 0x77;
    buf_index += 8;
    if (buf_index >= 0x3f0) {
      err = ftdi_write (buf, buf_index, &buf_index);
      if (err != 0) {
        return -1;
      }
      buf_index = 0;
    }
  }

  if (buf_index != 0) {
    err = ftdi_write (buf, buf_index, &buf_index);
    if (err != 0) {
      return -1;
    }
  }
  return 0;
}

static int fpga_config_read (uint16_t address,
                             void *data,
                             uint16_t count,
                             uint16_t flags)
{
  int err;
  uint8_t *buf;
  uint16_t cur_addr;
  int buf_index;
  int i;

  if (count == 0 ||
      (address + count) > 0x1000) {
    return -1;
  }

  memset (data, 0, count);

  buf = malloc (0x20000);
  if (buf == NULL) {
    return -1;
  }
  memset (buf, 0, 0x20000);

  buf_index = 0;
  for (cur_addr = address & 0xfffe;
       cur_addr < (address + count);
       cur_addr += 2) {
    buf[buf_index + 4] = (cur_addr | (flags & 0xC000)) >> 8;
    buf[buf_index + 5] = cur_addr & 0xff;
    buf[buf_index + 6] = 0x10 | (flags & 0x03);
    buf[buf_index + 7] = 0x77;
    buf_index += 8;
    if (buf_index >= 0x3f0) {
      err = ftdi_write (buf, buf_index, &buf_index);
      if (err != 0) {
        goto out;
      }
      buf_index = 0;
    }
  }
  if (buf_index != 0) {
      err = ftdi_write (buf, buf_index, &buf_index);
      if (err != 0) {
        goto out;
      }
  }

  usleep (MS_TO_US(10));

 retry:
  err = ftdi_read (buf, 0x20000, &buf_index);
  if (err < 0) {
    goto out;
  }

  for (i = 0; i < buf_index; i += 32) {
    int j;
    uint32_t status_field;
    uint32_t *data_fields;

    while (*(uint32_t *)(buf + i) == 0x55556666) {
      /*
       * Skip over FTDI workaround dummy fillers.
       */
      i += 4;
      if ((i + 32) > buf_index) {
        err = -1;
        goto retry;
      }
    }

    status_field = *(uint32_t *)(buf + i);
    data_fields = (uint32_t *)(buf + i + 4);

    if ((status_field & 0xf0000000) != 0xe0000000) {
      continue;
    }

    for (j = 0; j < 7; j++) {
      bool f_match;
      uint32_t data_field;

      f_match = (status_field & 0x0f) == (flags & 0x03);
      data_field = *data_fields++;
      status_field >>= 4;
      if (!f_match) {
        continue;
      }

      cur_addr = bswap_16 ((uint16_t) data_field);
      cur_addr -= (flags & 0xC000) + address;
      if (cur_addr == 0xffff) {
        /*
         * First unaligned byte.
         */
        *(uint8_t *) data = (data_field >> 24) & 0xff;
      }
      if (cur_addr >= count) {
        /*
         * Address read is out of range.
         */
        continue;
      }
      if (cur_addr == count - 1) {
        /*
         * Last byte.
         */
        *(((uint8_t *) data) + cur_addr) = (data_field >> 16) & 0xff;
      } else {
        /*
         * Normal two bytes.
         */
        *(((uint16_t *) data) + cur_addr) = (data_field >> 16) & 0xffff;
      }
    }
  }
 out:
  free (buf);
  return err;
}


static void hex_dump(uint8_t *buffer,
                     int num_bytes,
                     int line_length) {
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

typedef enum {
  STATE_STATUS,
  STATE_DATA,
  STATE_REM,
  STATE_TLP_COMPLETE,
} tlp_process_state_t;

typedef enum {
  TLP_NO_DATA,
  TLP_OUT_OF_SYNC,
  TLP_COMPLETE,
  TLP_CORRUPT,
} tlp_process_result_t;

typedef struct {
  uint32_t *p;
  uint32_t *e;
  tlp_process_state_t state;
  uint32_t status_field;
  int status_index;
} tlp_process_context;

static
tlp_process_result_t fpga_process_tlp (tlp_process_context *c,
                                       void **tlp_data,
                                       uint32_t *tlp_size)
{
  int err;
  int tlp_dword_index;

  tlp_dword_index = 0;
  while (1) {
    if (c->p == c->e) {
      int transferred;

      transferred = 0;
      err = ftdi_read (data, sizeof (data), &transferred);
      if (err != 0) {
        continue;
      }
      if ((transferred % sizeof (uint32_t)) != 0) {
        fprintf (stderr, "Transfer size not aligned to 32 bits\n");
      }
      transferred /= sizeof (uint32_t);

      c->p = (void *) data;
      c->e = c->p + transferred;

      if (c->p == c->e) {
        if (c->state == STATE_STATUS) {
          /*
           * Can only do this if we're not in the middle
           * of processing a TLP.
           */
          break;
        }

        continue;
      }
    }

    while (1) {
      if (c->state == STATE_STATUS) {
        c->status_index = 0;
        while (*c->p == 0x55556666 &&
               c->p < c->e) {
          c->p++;
        }

        if (c->p == c->e) {
          break;
        }

        c->status_field = *c->p++;
        if ((c->status_field & 0xf0000000) != 0xe0000000) {
          return TLP_OUT_OF_SYNC;
        } else {
            c->state = STATE_DATA;
        }
      } else if (c->state == STATE_DATA) {
        if (c->status_index == 7) {
          c->state = STATE_STATUS;
          continue;
        }

        if (c->p == c->e) {
          break;
        }

        c->status_index++;
        if ((c->status_field & 0x03) == 0x00) {
          /*
           * PCIe TLP.
           */
          tlp_dwords[tlp_dword_index++] = *c->p++;
        }
        if ((c->status_field & 0x07) == 0x04) {
          /*
           * PCIe TLP and LAST.
           */
          c->state = STATE_TLP_COMPLETE;
        }

        c->status_field >>= 4;
      } else if (c->state == STATE_REM) {
        if (c->status_index == 7) {
          c->state = STATE_STATUS;
          continue;
        }

        if (c->p == c->e) {
          break;
        }

        c->p++;
        c->status_index++;
        c->status_field >>= 4;
      } else if (c->state == STATE_TLP_COMPLETE) {
        if ((c->status_field & 0x03) == 0) {
          c->state = STATE_DATA;
        } else {
          c->state = STATE_REM;
        }

        if (tlp_dword_index >= 3 &&
            tlp_dword_index <= TLP_RX_MAX_SIZE_IN_DWORDS) {
          *tlp_data = tlp_dwords;
          *tlp_size = tlp_dword_index << 2;
          return TLP_COMPLETE;
        }

        return TLP_CORRUPT;
      }
    }
  }

  return TLP_NO_DATA;
}

static int ftdi_get (unsigned long index)
{
  libusb_device *device;
  libusb_device **device_list;
  struct libusb_device_descriptor desc;
  ssize_t device_count;
  ft60x_config chip_config;
  unsigned i;
  bool found;
  int err;

   device_count = libusb_get_device_list (usb_ctx, &device_list);
  if (device_count < 0) {
    fprintf (stderr, "device_count < 0\n");
    return -1;
  }

  found = false;
  for (i = 0; i < device_count; i++) {
    device = device_list[i];

    err = libusb_get_device_descriptor (device, &desc);
    if (err != 0) {
      fprintf (stderr, "libusb_get_device_descriptor[%d]: %s\n", i, libusb_strerror (err));
      continue;
    }

    if (desc.idVendor == FTDI_VENDOR_ID &&
        desc.idProduct == FTDI_FT60X_PRODUCT_ID) {
      if (index) {
        index--;
        continue;
      }

      printf ("Using device %04x:%04x (bus %d, device %d)\n",
              desc.idVendor,
              desc.idProduct,
              libusb_get_bus_number (device),
              libusb_get_device_address (device));
      found = true;
      break;
    }
  }

  if (!found) {
    return -1;
  }

  err = libusb_open (device, &device_handle);
  if (err != 0) {
    fprintf (stderr, "libusb_open: %s\n", libusb_strerror (err));
    return -1;
  }

  err = libusb_kernel_driver_active (device_handle, FTDI_COMMUNICATION_INTERFACE);
  if (err < 0) {
    fprintf (stderr, "libusb_kernel_driver_active(FTDI_COMMUNICATION_INTERFACE): %s\n",
             libusb_strerror (err));
    return -1;
  }
  if (err != 0) {
    fprintf (stderr, "FTDI_COMMUNICATION_INTERFACE already in use.\n");
    return -1;
  }

  err = libusb_claim_interface (device_handle, FTDI_COMMUNICATION_INTERFACE);
  if (err != 0) {
    fprintf (stderr, "libusb_claim_interface(FTDI_COMMUNICATION_INTERFACE): %s\n",
             libusb_strerror(err));
    return -1;
  }

  err = libusb_kernel_driver_active (device_handle, FTDI_DATA_INTERFACE);
  if (err < 0) {
    fprintf (stderr, "libusb_kernel_driver_active(FTDI_DATA_INTERFACE): %s\n",
             libusb_strerror (err));
    return -1;
  }
  if (err != 0) {
    fprintf (stderr, "FTDI_DATA_INTERFACE already in use.\n");
    return -1;
  }

  err = libusb_claim_interface (device_handle, FTDI_DATA_INTERFACE);
  if (err != 0) {
    fprintf (stderr, "libusb_claim_interface(FTDI_DATA_INTERFACE): %s\n",
             libusb_strerror(err));
    return -1;
  }

  err = ftdi_get_config (&chip_config) ;
  if (err < 0) {
    fprintf (stderr, "ftdi_get_config: %s\n", libusb_strerror (err));
    return -1;
  }
  if (err != sizeof (chip_config)) {
    fprintf (stderr, "ftdi_get_config: bad config size\n");
    return -1;
  }

  if (chip_config.fifo_mode != CONFIGURATION_FIFO_MODE_245 ||
      chip_config.channel_config != CONFIGURATION_CHANNEL_CONFIG_1 ||
      chip_config.optional_feature_support != CONFIGURATION_OPTIONAL_FEATURE_DISABLE_ALL) {
    fprintf (stderr, "Fixing bad FTDI configuration\n");
    chip_config.fifo_mode = CONFIGURATION_FIFO_MODE_245;
    chip_config.channel_config = CONFIGURATION_CHANNEL_CONFIG_1;
    chip_config.optional_feature_support = CONFIGURATION_OPTIONAL_FEATURE_DISABLE_ALL;

    err = ftdi_set_config (&chip_config);
    if (err < 0) {
      fprintf (stderr, "ftdi_set_config: %s\n", libusb_strerror (err));
      return -1;
    }
    if (err != sizeof (chip_config)) {
      fprintf (stderr, "ftdi_set_config: bad config size\n");
      return -1;
    }
  }

  return 0;
}

static int parse_opts (int argc, char **argv,
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

int main (int argc, char **argv)
{
  int err;
  unsigned long device_index;
  uint8_t fpga_version;
  uint8_t pcie_core;
  char *remote_addr;
  in_port_t remote_port;
  int socket_fd;
  struct sockaddr_in sa;
  tlp_process_context context;

  device_index = 0;
  remote_addr = "127.0.0.1";
  remote_port = 9999;
  err = parse_opts (argc, argv, &device_index,
                    &remote_addr, &remote_port);
  if (err != 0) {
    return -1;
  };

  printf ("UDP server is %s:%u\n", remote_addr, remote_port);

  socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_fd < 0) {
    fprintf(stderr, "socket: %s\n", strerror (errno));
    return -1;
  }

  sa.sin_family = AF_INET;
  sa.sin_port = htons(remote_port);
  sa.sin_addr.s_addr = inet_addr(remote_addr);

  err = libusb_init (&usb_ctx);
  if (err != 0) {
    fprintf (stderr, "libusb_init: %s\n", libusb_strerror (err));
    return -1;
  }

  err = ftdi_get (device_index);
  if (err != 0) {
    fprintf (stderr, "No FTDI device found\n");
    return -1;
  }

  err = fpga_config_read (0x0008, &fpga_version, 1,
                          FPGA_REG_CORE | FPGA_REG_READONLY);
  if (err != 0) {
    return -1;
  }

  if (fpga_version != 4) {
    fprintf (stderr, "Unsupported FPGA version 0x%x\n",
             fpga_version);
    return -1;
  }

  err = fpga_config_read (0x0019, &pcie_core, 1,
                          FPGA_REG_CORE | FPGA_REG_READWRITE);
  if (err != 0) {
    return -1;
  }

  /*
   * Disable CFGTLP FILTER TLP FROM USER to see accesses
   * to parts of the CFG space beyond what's managed by
   * the Xilinx IP.
   */
  pcie_core &= ~0x10;
  err = fpga_config_write (0x0019, &pcie_core, 1,
                           FPGA_REG_CORE | FPGA_REG_READWRITE);
  if (err != 0) {
    return -1;
  }

  err = fpga_config_read (0x0019, &pcie_core, 1,
                          FPGA_REG_CORE | FPGA_REG_READWRITE);
  if (err != 0) {
    return -1;
  }

  if ((pcie_core & 0x10) != 0) {
    fprintf (stderr, "Couldn't clear CFGTLP FILTER TLP FROM USER\n");
    return -1;
  }

  memset (&context, 0, sizeof (context));
  while (1) {
    void *tlp_data;
    uint32_t tlp_size;
    tlp_process_result_t state;

    state = fpga_process_tlp (&context, &tlp_data, &tlp_size);
    if (state == TLP_OUT_OF_SYNC) {
      fprintf (stderr, "Missing header\n");
    } else if (state == TLP_CORRUPT) {
      fprintf (stderr, "Bad PCIe TLP received\n");
    } else if (state == TLP_COMPLETE) {
      if (verbose) {
        printf ("TLP of 0x%x bytes\n", tlp_size);
        hex_dump (tlp_data, tlp_size, 16);
      }

      err = sendto(socket_fd, tlp_data, tlp_size,
                   0, (struct sockaddr *) &sa,
                   sizeof (sa));
      if (err < 0)  {
        fprintf (stderr, "sendto: %s\n", strerror (errno));
      }
    }
  }

  return 0;
}

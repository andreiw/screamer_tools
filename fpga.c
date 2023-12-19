/*
 * Based on:
 * - https://github.com/ufrisk/LeechCore/blob/master/leechcore/device_fpga.c (GPLv3)
 *
 * SPDX-License-Identifier: GPL-3.0
 *
 * TBD:
 * - not endian safe (assumes LE)
 * - sketchy buffer handling in fpga_config_read
 *   (0x400 should be enough, or just reuse the
 *   global data buffer).
 * - BAR enable.
 */

#include "screamer.h"

#define FPGA_CMD_VERSION_MAJOR          0x01
#define FPGA_CMD_DEVICE_ID              0x03
#define FPGA_CMD_VERSION_MINOR          0x05

#define FPGA_REG_CORE                 0x0003
#define FPGA_REG_PCIE                 0x0001
#define FPGA_REG_READONLY             0x0000
#define FPGA_REG_READWRITE            0x8000
#define FPGA_REG_SHADOWCFGSPACE       0xC000

#define TLP_RX_MAX_SIZE             (16+1024)
#define TLP_RX_MAX_SIZE_IN_DWORDS   ((int) (TLP_RX_MAX_SIZE / sizeof (uint32_t)))

static uint8_t data[TLP_RX_MAX_SIZE];
static uint32_t tlp_dwords[TLP_RX_MAX_SIZE_IN_DWORDS];

int
fpga_init (void)
{
  int err;
  uint8_t fpga_version;
  uint8_t pcie_core;

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

  return 0;
}

int
fpga_config_write (uint16_t address,
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

int
fpga_config_read (uint16_t address,
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

      cur_addr = be16toh ((uint16_t) data_field);
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

tlp_process_result_t
fpga_process_tlp (tlp_process_context *c,
                  void **tlp_data,
                  uint32_t *tlp_size)
{
  int err;
  int tlp_dword_index;
  int tlp_dword_count;
  bool tlp_header_seen;

  tlp_dword_index = 0;
  tlp_dword_count = 0;
  tlp_header_seen = false;

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
        if (!tlp_header_seen) {
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
          tlp_dwords[tlp_dword_index] = *c->p++;

          if (!tlp_header_seen) {
            uint32_t len_dw;
            uint32_t dw = tlp_dwords[tlp_dword_index];

            len_dw = tlp_packet_len_dws (dw, NULL);
            tlp_dword_count += len_dw;

            if (len_dw > 1) {
              tlp_header_seen = true;
            }
          }
          tlp_dword_index++;
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

        if (tlp_dword_index == tlp_dword_count) {
          *tlp_data = tlp_dwords;
          *tlp_size = tlp_dword_index << 2;
          return TLP_COMPLETE;
        }

        fprintf (stderr, "Disagreement on TLP size (header -> %u dw, ctual -> %u dw\n",
                tlp_dword_count, tlp_dword_index);
        return TLP_CORRUPT;
      }
    }
  }

  return TLP_NO_DATA;
}

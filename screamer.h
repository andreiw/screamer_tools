#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <inttypes.h>
#include <string.h>
#include <endian.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include "ft60x.h"

#define MS_TO_US(x) ((x) * 1000)

int
ftdi_set_config (ft60x_config *config);

int
ftdi_get_config (ft60x_config *config);

int
ftdi_send_cmd_read (int size);

int
ftdi_write (void *data,
            int size,
            int *transferred);

int
ftdi_read (void *data,
           int size,
           int *transferred);

int
ftdi_get (unsigned long index);

int
fpga_init (void);

int
fpga_config_write (uint16_t address,
                   void *data,
                   uint16_t count,
                   uint16_t flags);

int
fpga_config_read (uint16_t address,
                  void *data,
                  uint16_t count,
                  uint16_t flags);

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

#define TLP_MRd32       0x00
#define TLP_MRd64       0x20
#define TLP_MRdLk32     0x01
#define TLP_MRdLk64     0x21
#define TLP_MWr32       0x40
#define TLP_MWr64       0x60
#define TLP_IORd        0x02
#define TLP_IOWr        0x42
#define TLP_CfgRd0      0x04
#define TLP_CfgRd1      0x05
#define TLP_CfgWr0      0x44
#define TLP_CfgWr1      0x45
#define TLP_Cpl         0x0A
#define TLP_CplD        0x4A
#define TLP_CplLk       0x0B
#define TLP_CplDLk      0x4B

typedef union {
  uint32_t dw;
  struct {
    uint16_t _length : 10;
    uint16_t _at : 2;
    uint16_t _attr : 2;
    uint16_t _ep : 1;
    uint16_t _td : 1;
    uint8_t _r1 : 4;
    uint8_t _tc : 3;
    uint8_t _r2 : 1;
    union {
      uint8_t _fmt_type;
      struct {
        uint8_t _type : 5;
        uint8_t _fmt : 3;
      };
    };
  };
} tlp_header_t;

typedef struct {
  tlp_header_t tlp_header;
  uint8_t first_be : 4;
  uint8_t last_be : 4;
  uint8_t tag;
  uint16_t requester_id;
  uint8_t r1 : 2;
  uint8_t reg_num : 6;
  uint8_t ext_reg_num : 4;
  uint8_t r2 : 4;
  uint8_t function_num : 3;
  uint8_t device_num : 3;
  uint8_t bus_num;
} tlp_cfg_t;

typedef union {
  tlp_header_t header;
  tlp_cfg_t cfg;
} tlp_t;

_Static_assert (sizeof (tlp_header_t) == sizeof (uint32_t),
                "sizeof (tlp_header)");


int
tlp_packet_len_dws (uint32_t raw_leader,
                    int *payload_len_dws);

void *
tlp_packet_to_host (void *data,
                    tlp_t *tlp,
                    int *payload_len_dws);

unsigned
tlp_cfg_reg (tlp_cfg_t *cfg);

bool
tlp_dw_is_prefix (uint32_t dw);

int
tlp_packet_len_dws (uint32_t raw_leader,
                    int *payload_len_dws);

tlp_process_result_t
fpga_process_tlp (tlp_process_context *c,
                  void **tlp_data,
                  uint32_t *tlp_size);

void hex_dump(uint8_t *buffer,
              int num_bytes,
              int line_length);

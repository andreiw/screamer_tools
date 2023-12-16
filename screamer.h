#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <inttypes.h>
#include <string.h>
#include <byteswap.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
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

bool
tlp_dw_is_prefix (uint32_t dw);

int
tlp_header_count (uint32_t dw);  

tlp_process_result_t
fpga_process_tlp (tlp_process_context *c,
                  void **tlp_data,
                  uint32_t *tlp_size);

void hex_dump(uint8_t *buffer,
              int num_bytes,
              int line_length);

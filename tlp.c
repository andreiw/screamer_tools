#include "screamer.h"

bool
tlp_dw_is_prefix (uint32_t dw)
{
  uint8_t dw0 = (uint8_t) dw;
  uint8_t fmt = dw0 >> 5;

  if (fmt == 4) {
    /*
     * Prefix.
     */
    return true;
  }

  return false;
}

bool
tlp_is_cw0 (void *tlp_data, void **header)
{
  uint32_t *d;
  uint8_t dw0;
  uint8_t fmt;
  uint8_t type;

  d = tlp_data;
  while (tlp_dw_is_prefix (*d)) {
    d++;
  }

  dw0 = (uint8_t) *d;
  fmt = dw0 >> 5;
  type = dw0 & 0x1f;

  if (fmt == 2 && type == 0x4) {
    *header = d;
    return true;
  }

  return false;
}

unsigned
tlp_cfg_reg (void *tlp_header)
{
  unsigned reg;
  uint32_t dw0;
  uint8_t *db;

  dw0 = ((uint32_t *) tlp_header)[2];
  db = (void *) &dw0;
  reg = db[3] | ((int) (db[2] & 0xf)) << 8;

  return reg;
}

uint32_t
tlp_cfg_be (void *tlp_header)
{
  uint32_t dw1;
  uint8_t *db;

  dw1 = ((uint32_t *) tlp_header)[1];
  db = (void *) &dw1;

  return db[3] & 0xf;
}

uint32_t
tlp_cfg_data (void *tlp_header)
{
  return ((uint32_t *) tlp_header)[3];
}

int
tlp_header_count (uint32_t dw)
{
  int count;
  uint8_t *db;
  uint8_t fmt;
  int len;
  int digest;

  db = (void *) &dw;
  fmt = (db[0] >> 5) & 3;
  len = db[3] | ((int) (db[2] & 0x3)) << 8;
  digest = db[2] >> 7;

  if (len == 0) {
    len = 0x400;
  }

  switch (fmt) {
  case 0:
    count = 3;
    break;
  case 1:
    count = 4;
    break;
  case 2:
    count = 3 + len;
    break;
  case 3:
    count = 4 + len;
  }

  return count + digest;
}

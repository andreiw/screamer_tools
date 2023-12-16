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

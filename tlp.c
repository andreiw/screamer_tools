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

unsigned
tlp_cfg_reg (tlp_cfg_t *cfg)
{
  return ((unsigned) cfg->ext_reg_num << 8) +
    ((unsigned) cfg->reg_num << 2);
}

int
tlp_packet_len_dws (uint32_t raw_leader,
                    int *payload_len_dws)
{
  int len;
  int count;
  tlp_header_t tlp_header;

  tlp_header.dw = be32toh (raw_leader);
  if (tlp_header._fmt == 4) {
    /*
     * Prefix, which we ignore here.
     */
    return 1;
  }

  count = 0;
  len = tlp_header._length;
  if (len == 0) {
    len = 0x400;
  }

  switch (tlp_header._fmt) {
  case 0:
    count = 3;
    assert (len == 0);
    break;
  case 1:
    count = 4;
    assert (len == 0);
    break;
  case 2:
    count = 3 + len;
    break;
  case 3:
    count = 4 + len;
  }

  if (payload_len_dws != NULL) {
    *payload_len_dws = len;
  }

  return count + tlp_header._td;
}

void *
tlp_packet_to_host (void *data,
                    tlp_t *tlp,
                    int *payload_len_dws)
{
  int i;
  int len_dws;
  uint32_t *s = data;
  uint32_t *d = (void *) tlp;

  *payload_len_dws = 0;
  while ((len_dws =
          tlp_packet_len_dws (*s, payload_len_dws)) == 1) {
    s++;
  }

  /*
   * s now points to the TLP header raw data.
   */
  len_dws -= *payload_len_dws;
  for (i = 0; i < len_dws; i++) {
    *d++ = be32toh (*s++);
  }

  return s;
}

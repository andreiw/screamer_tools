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
tlp_hdr_len_dws (tlp_header_t *tlp_header,
                 int *payload_len_dws)
{
  int len;
  int count;

  if (tlp_header->fmt == 4) {
    /*
     * Prefix, which we ignore here.
     */
    if (payload_len_dws != NULL) {
      *payload_len_dws = 0;
    }
    return 1;
  }

  count = 0;
  len = tlp_header->length;
  if (len == 0) {
    len = 0x400;
  }

  switch (tlp_header->fmt) {
  case 0:
    count = 3;
    if (payload_len_dws != NULL) {
      *payload_len_dws = 0;
    }
    break;
  case 1:
    count = 4;
    if (payload_len_dws != NULL) {
      *payload_len_dws = 0;
    }
    break;
  case 2:
    count = 3 + len;
    if (payload_len_dws != NULL) {
      *payload_len_dws = len;
    }
    break;
  case 3:
    count = 4 + len;
    if (payload_len_dws != NULL) {
      *payload_len_dws = len;
    }
  }

  return count + tlp_header->td;
}

int
tlp_packet_len_dws (uint32_t raw_leader,
                    int *payload_len_dws)
{
  tlp_header_t tlp_header;

  tlp_header._dw = be32toh (raw_leader);
  return tlp_hdr_len_dws (&tlp_header, payload_len_dws);
}

void *
tlp_host_to_packet (tlp_t *tlp,
                    void *out,
                    int out_size)
{
  int i;
  int len_dws;
  int payload_len_dws;
  uint32_t *s = (void *) tlp;
  uint32_t *d = out;

  len_dws = tlp_hdr_len_dws (&(tlp->hdr), &payload_len_dws);
  /*
   * tlp begins with a header, not a prefix.
   */
  assert (len_dws != 1);

  len_dws -= payload_len_dws;
  assert (len_dws <= (out_size * (int) sizeof (uint32_t)));
  assert (len_dws <= (int) sizeof (tlp_t));

  /*
   * Only swap the packet, not payload.
   */
  for (i = 0; i < len_dws; i++) {
    *d++ = htobe32 (*s++);
  }

  /*
   * Pointer to payload bytes, if any.
   */
  return d;
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
  assert (len_dws <= (int) sizeof (tlp_t));

  for (i = 0; i < len_dws; i++) {
    *d++ = be32toh (*s++);
  }

  return s;
}


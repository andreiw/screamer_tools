#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <libusb.h>
#include <event2/event.h>

#define FTDI_VENDOR_ID               0x0403
#define FTDI_FT60X_PRODUCT_ID        0x601f
#define FTDI_COMMUNICATION_INTERFACE 0x00
#define FTDI_DATA_INTERFACE          0x01
#define FTDI_ENDPOINT_SESSION_OUT    0x01
#define FTDI_ENDPOINT_OUT            0x02
#define FTDI_ENDPOINT_IN             0x82

static libusb_context *usb_ctx;
static libusb_device_handle *device_handle;

int get_device (unsigned long index)
{
  libusb_device *device;
  libusb_device **device_list;
  struct libusb_device_descriptor desc;
  ssize_t device_count;
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

  return 0;
}

int parse_opts (int argc, char **argv,
                unsigned long *device_index)
{
  int opt;

  while ((opt = getopt(argc, argv, "n:")) != -1) {
    switch (opt) {
    case 'n':
      *device_index = strtoul (optarg, NULL, 10);
      break;
    default: /* '?' */
      fprintf(stderr, "Usage: %s [-n device_index]\n",
              argv[0]);
      return -1;
    }
  }

  return 0;
}

int main (int argc, char **argv)
{
  int err;
  unsigned long device_index;

  err = parse_opts (argc, argv, &device_index);
  if (err != 0) {
    return -1;
  };

  err = libusb_init (&usb_ctx);
  if (err != 0) {
    fprintf (stderr, "libusb_init: %s\n", libusb_strerror (err));
    return -1;
  }

  err = get_device (device_index);
  if (err != 0) {
    fprintf (stderr, "No FTDI device found\n");
    return -1;
  }

  return 0;
}

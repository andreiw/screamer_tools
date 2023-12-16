/*
 * Based on:
 * - https://github.com/ufrisk/LeechCore-plugins/tree/master/leechcore_ft601_driver_linux (GPLv3)
 *
 * SPDX-License-Identifier: GPL-3.0
 *
 */

#include "screamer.h"
#include <libusb.h>

static libusb_context *usb_ctx;
static libusb_device_handle *device_handle;

int
ftdi_set_config (ft60x_config *config)
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

int
ftdi_get_config (ft60x_config *config)
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

int
ftdi_send_cmd_read (int size)
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

int
ftdi_write (void *data, int size, int *transferred)
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

int
ftdi_read (void *data, int size, int *transferred)
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

int
ftdi_get (unsigned long index)
{
  libusb_device *device;
  libusb_device **device_list;
  struct libusb_device_descriptor desc;
  ssize_t device_count;
  ft60x_config chip_config;
  unsigned i;
  bool found;
  int err;

  err = libusb_init (&usb_ctx);
  if (err != 0) {
    fprintf (stderr, "libusb_init: %s\n", libusb_strerror (err));
    return -1;
  }

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

#include <stdio.h>
#include <libusb.h>
// #include <event2/event.h>

int main(int argc, char **argv)
{
  int err;
  libusb_context *usb_ctx;

  err = libusb_init(&usb_ctx);
  if (err != 0) {
    fprintf (stderr, "libusb_init: %s\n", libusb_strerror (err));
    return 1;
  }

  return 0;
}

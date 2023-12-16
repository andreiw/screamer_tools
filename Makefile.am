PKGLIBS = libusb libevent

bin_PROGRAMS = screamer_scope
screamer_scope_SOURCES = scope.c ftdi.c fpga.c util.c tlp.c
screamer_scope_CFLAGS = -Wall -Wextra
screamer_scope_CPPFLAGS = @LUSB_CFLAGS@
screamer_scope_LDADD = @LUSB_LIBS@

PKGLIBS = libusb libevent

COMMON_CPPFLAGS = @LUSB_CFLAGS@
COMMON_LIBS = @LUSB_LIBS@
COMMON_SOURCES = ftdi.c fpga.c util.c tlp.c
COMMON_FLAGS = -Wall -Wextra

bin_PROGRAMS = screamer_scope screamer_sac

screamer_scope_SOURCES = scope.c $(COMMON_SOURCES)
screamer_scope_CFLAGS = $(COMMON_FLAGS)
screamer_scope_CPPFLAGS = $(COMMON_CPPFLAGS)
screamer_scope_LDADD = $(COMMON_LIBS)

screamer_sac_SOURCES = sac.c $(COMMON_SOURCES)
screamer_sac_CFLAGS = $(COMMON_FLAGS)
screamer_sac_CPPFLAGS = $(COMMON_CPPFLAGS)
screamer_sac_LDADD = $(COMMON_LIBS)

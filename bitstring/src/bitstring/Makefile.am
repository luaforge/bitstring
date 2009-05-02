INCLUDES = -I$(top_builddir)/src
AM_LDFLAGS = -L.

EXTRA_DIST = 
EXTRA_DIST += lhexdump.c
EXTRA_DIST += lbindump.c


lib_LTLIBRARIES = bitstring.la
bitstring_la_SOURCES = lbitstring.c
bitstring_la_SOURCES += bitstring.h
bitstring_la_LIBADD = -llua -lm 
bitstring_la_LDFLAGS = -module -version-info 0:0:0 -release $(BITSTRING_VERSION)

lib_LTLIBRARIES += libbitstring.la
libbitstring_la_SOURCES = lbitstring.c
libbitstring_la_SOURCES += bitstring.h
libbitstring_la_LIBADD = -llua -lm 
libbitstring_la_LDFLAGS = -version-info 0:0:0 -release $(BITSTRING_VERSION)


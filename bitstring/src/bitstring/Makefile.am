INCLUDES = -I$(top_builddir)/src
AM_LDFLAGS = -L.


lib_LTLIBRARIES = libbitstring.la
libbitstring_la_SOURCES = bitstring.h \
						  lbitstring.c

libbitstring_la_LIBADD = -llua -lm 

libbitstringincludedir = $(includedir)/bitstring
libbitstringinclude_HEADERS = bitstring.h

bitstring_la_LDFLAGS = -module -version-info 0:0:0 -release $(BITSTRING_VERSION)



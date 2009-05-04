INCLUDES = -I$(top_builddir)/src 
check_PROGRAMS = test_bitstring 
check_SCRIPTS = test_bitstring.sh 
TESTS = $(check_SCRIPTS) $(check_PROGRAMS) 

EXTRA_DIST = 
EXTRA_DIST += test_bitstring.sh
EXTRA_DIST += test_helpers.lua
EXTRA_DIST += test_bitstring.lua
EXTRA_DIST += test_hexdump.lua
EXTRA_DIST += test_bindump.lua
EXTRA_DIST += test_compile.lua
EXTRA_DIST += test_profiler.lua

test_bitstring_SOURCES = test_bitstring.c
test_bitstring_LDADD = -lbitstring
test_bitstring_LDFLAGS = -L$(top_builddir)/src/bitstring/.libs


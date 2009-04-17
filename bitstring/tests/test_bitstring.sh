#!/bin/sh

rm -f bitstring.so
ln -s ../src/bitstring/.libs/libbitstring.so bitstring.so
lua ./test_bitstring.lua



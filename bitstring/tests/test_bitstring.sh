#!/bin/sh

rm -f bitstring.so
rm -f libbitstring.so
ln -s ../src/bitstring/.libs/bitstring.so libbitstring.so
ln -s ../src/bitstring/.libs/bitstring.so bitstring.so

LUA=lua
TESTS="test_bitstring \
       test_hexdump\
       test_bindump\
       test_compile\
       test_profiler"

for test_name in $TESTS; do
    test='./'$test_name'.lua'
    $LUA $test
    result=$?
    if [ $result -ne 0 ]; then 
        echo $test_name failed with $result
        exit $result;
    fi
done



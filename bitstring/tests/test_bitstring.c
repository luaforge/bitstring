#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <bitstring/bitstring.h>


static const char *cmd = 
"rm -f bitstring.so && "
"ln -s ../src/bitstring/.libs/libbitstring.so bitstring.so && "
"lua ./test_bitstring.lua";


int main (int argc, char **argv)
{
    int result = WEXITSTATUS(system(cmd));
    return result;
}


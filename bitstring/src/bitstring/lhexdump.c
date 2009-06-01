/* 
 * Copyright (c) 2009, Giora Kosoi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Giora Kosoi ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Giora Kosoi BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define HEX_BYTES_IN_ROW 16
#define HEX_HALF_SEPARATOR_WIDTH 2
#define HEX_BYTES_FROM_TEXT_WIDTH 4
#define HEX_OFFSET_WIDTH 10

#define HEX_PRINTED_LINE_LENGTH (HEX_OFFSET_WIDTH + \
                             HEX_BYTES_IN_ROW * 4 + \
                             HEX_HALF_SEPARATOR_WIDTH + \
                             HEX_BYTES_FROM_TEXT_WIDTH + 1)

static const char *HEX_BYTES[256] = 
{
    "00 ","01 ","02 ","03 ","04 ","05 ","06 ","07 ",
    "08 ","09 ","0a ","0b ","0c ","0d ","0e ","0f ",
    "10 ","11 ","12 ","13 ","14 ","15 ","16 ","17 ",
    "18 ","19 ","1a ","1b ","1c ","1d ","1e ","1f ",
    "20 ","21 ","22 ","23 ","24 ","25 ","26 ","27 ",
    "28 ","29 ","2a ","2b ","2c ","2d ","2e ","2f ",
    "30 ","31 ","32 ","33 ","34 ","35 ","36 ","37 ",
    "38 ","39 ","3a ","3b ","3c ","3d ","3e ","3f ",
    "40 ","41 ","42 ","43 ","44 ","45 ","46 ","47 ",
    "48 ","49 ","4a ","4b ","4c ","4d ","4e ","4f ",
    "50 ","51 ","52 ","53 ","54 ","55 ","56 ","57 ",
    "58 ","59 ","5a ","5b ","5c ","5d ","5e ","5f ",
    "60 ","61 ","62 ","63 ","64 ","65 ","66 ","67 ",
    "68 ","69 ","6a ","6b ","6c ","6d ","6e ","6f ",
    "70 ","71 ","72 ","73 ","74 ","75 ","76 ","77 ",
    "78 ","79 ","7a ","7b ","7c ","7d ","7e ","7f ",
    "80 ","81 ","82 ","83 ","84 ","85 ","86 ","87 ",
    "88 ","89 ","8a ","8b ","8c ","8d ","8e ","8f ",
    "90 ","91 ","92 ","93 ","94 ","95 ","96 ","97 ",
    "98 ","99 ","9a ","9b ","9c ","9d ","9e ","9f ",
    "a0 ","a1 ","a2 ","a3 ","a4 ","a5 ","a6 ","a7 ",
    "a8 ","a9 ","aa ","ab ","ac ","ad ","ae ","af ",
    "b0 ","b1 ","b2 ","b3 ","b4 ","b5 ","b6 ","b7 ",
    "b8 ","b9 ","ba ","bb ","bc ","bd ","be ","bf ",
    "c0 ","c1 ","c2 ","c3 ","c4 ","c5 ","c6 ","c7 ",
    "c8 ","c9 ","ca ","cb ","cc ","cd ","ce ","cf ",
    "d0 ","d1 ","d2 ","d3 ","d4 ","d5 ","d6 ","d7 ",
    "d8 ","d9 ","da ","db ","dc ","dd ","de ","df ",
    "e0 ","e1 ","e2 ","e3 ","e4 ","e5 ","e6 ","e7 ",
    "e8 ","e9 ","ea ","eb ","ec ","ed ","ee ","ef ",
    "f0 ","f1 ","f2 ","f3 ","f4 ","f5 ","f6 ","f7 ",
    "f8 ","f9 ","fa ","fb ","fc ","fd ","fe ","ff "
};

static int l_hexdump(lua_State *l)
{
    size_t len = 0;
    const unsigned char *input = get_substring(l, &len, 1, 2, 3);

    luaL_Buffer b; 
    luaL_buffinit(l, &b);

    size_t i = 0;
    while(i < len)
    {
        unsigned char *result = (unsigned char *)luaL_prepbuffer(&b);
        size_t column = 0;
        while(i < len && column < LUAL_BUFFERSIZE - HEX_PRINTED_LINE_LENGTH)
        {
            size_t line_start = i;
            sprintf((char*)(result + column), "%08x: ", line_start);
            column += HEX_OFFSET_WIDTH;
            size_t k = 0;
            while(k < HEX_BYTES_IN_ROW / 2 && i < len)
            {
                memcpy(result + column, HEX_BYTES[input[i]], 3);
                column += 3;
                ++i; ++k;
            }
            memset(result + column, ' ', HEX_HALF_SEPARATOR_WIDTH);
            column += HEX_HALF_SEPARATOR_WIDTH;

            while(k < HEX_BYTES_IN_ROW && i < len)
            {
                memcpy(result + column, HEX_BYTES[input[i]], 3);
                column += 3;
                ++i; ++k;
            }
            size_t space_length = HEX_BYTES_FROM_TEXT_WIDTH;
            if(k != HEX_BYTES_IN_ROW)
            {
                space_length += (HEX_BYTES_IN_ROW - k) * 3;
            }
            memset(result + column, ' ', space_length);
            column += space_length;
            k = 0;
            while(line_start + k < len && k < HEX_BYTES_IN_ROW)
            {
                unsigned char ch = input[line_start + k];
                result[column] = isprint(ch) ? ch : '.';
                ++k; ++column;
            }
            result[column] = '\n';
            ++column;
        }
        luaL_addsize(&b, column);
    }
    luaL_pushresult(&b);
    return 1;
}

static int l_hexstream(lua_State *l)
{
    size_t len = 0;
    const unsigned char *input = get_substring(l, &len, 1, 2, 3);

    luaL_Buffer b; 
    luaL_buffinit(l, &b);

    size_t i = 0;
    while(i < len)
    {
        unsigned char *result = (unsigned char *)luaL_prepbuffer(&b);
        size_t column = 0;
        while(i < len && column < LUAL_BUFFERSIZE - 2)
        {
            memcpy(result + column, HEX_BYTES[input[i]], 2);
            column += 2;
            ++i;
        }
        luaL_addsize(&b, column);
    }
    luaL_pushresult(&b);
    return 1;
}

static int l_fromhexstream(lua_State *l)
{
    size_t len = 0;
    const unsigned char *input = get_substring(l, &len, 1, 2, 3);

    luaL_Buffer b; 
    luaL_buffinit(l, &b);

    if(len % 2 != 0)
    {
        luaL_error(l, "wrong format: input must be hexstream with even number of digits");
    }

    size_t i = 0;
    while(i < len)
    {
        unsigned char *result = (unsigned char *)luaL_prepbuffer(&b);
        unsigned char *current_byte = result;
        unsigned char *result_end = result + LUAL_BUFFERSIZE;
        while(i < len && current_byte < result_end)
        {
            char byte[3] = {0, 0, 0};
            char *endptr = NULL;
            memcpy(byte, input + i, 2); 
            *current_byte = (unsigned char)strtol(byte, &endptr, 16);
            if(*endptr != '\0')
            {
                luaL_error(l, "wrong format: %s are not hexadecimal digits", byte);
            }
            ++current_byte;
            i += 2;
        }
        luaL_addsize(&b, current_byte - result);
    }
    luaL_pushresult(&b);
    return 1;
}


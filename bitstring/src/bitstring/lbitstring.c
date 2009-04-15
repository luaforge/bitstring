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

#include "config.h"

#include <lua.h>
#include <lauxlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdint.h>

/*
 * Element types
 */
typedef enum
{
    ET_UNDEFINED = 0,
    /* integer of up to lua_Integer * CHAR_BIT bits */
    ET_INTEGER,
    /* octet string. each octet may hold values of 0-255 */
    ET_BINARY,
} ELEMENT_TYPE;

/* 
 * type tokens 
 */
static const char *TYPES[] =
{
    "undefined",
    "int",
    "bin",
    NULL
};

/*
 * Element endianess
 */
typedef enum
{
    /* the default is big endian */
    EE_DEFAULT = 0,
    /* big endian */
    EE_BIG,
    /* little endian */
    EE_LITTLE,
} ELEMENT_ENDIANESS;

/*
 * endianess tokens
 */
static const char *ENDIANESSES[] =
{
    "default",
    "big",
    "little",
    NULL
};

/*
 * all size token
 * when all is specified as size all the binary string parameter should be packed
 * may be specified with binary strings only
 */
static const char *ALL_SPECIFIER = "all";
static const int ALL = -1;

/*
 * rest size token
 * when rest is specified as size all the rest of input binary string should be unpacked
 * may be specified with binary strings only
 */
static const char *REST_SPECIFIER = "rest";
static const int REST = -2;

/*
 * delimit element parts 
 */
static const char PART_DELIMITER = ':';

/*
 * delimit elements
 */
static const char *ELEMENT_DELIMITERS = ", \t\n";

/*
 * parse states 
 */ 
typedef enum
{
    SIZE_STATE = 1,
    TYPE_STATE,
    ENDIANESS_STATE,
    SPACE_STATE,
} PARSE_STATE;

/*
 * element data that is passed between element processing
 * functions
 */
typedef struct
{
    /* the size is in bits for integers and bytes for binary strings */
    size_t size;
    ELEMENT_TYPE type;
    ELEMENT_ENDIANESS endianess;
} ELEMENT_DESCRIPTION;

/*
 * pack state data that is passed between functions during packing
 */
typedef struct
{
    /* buffer for composing the result */
    luaL_Buffer *buffer;
    /* temporary buffer that is flushed into result buffer */
    unsigned char *prep_buffer;
    /* current bit location for packing */
    size_t current_bit;
    /* space in bits in the temporary prep_buffer */
    size_t result_bits;
} PACK_STATE;

/*
 * unpack state data that is passed or collected between functions during unpacking
 */
typedef struct
{
    /* number of return values the unpack function will return*/
    size_t return_count;
    /* current bit location for unpacking */
    size_t current_bit;
    /* count of bits left in the input string. decremented after each unpack_elem*/
    size_t source_bits;
    /* pointer to beginning of input string */
    const unsigned char *source;
    /* pointer to end of input string */
    const unsigned char *source_end;
} UNPACK_STATE;

/*
 * pointer to callback function that process elements
 */
typedef void (*ELEM_HANDLER)(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, void *arg);

/*
 * name
 *      bits_to_bytes
 *
 * parameters
 *      count_bits - number of bits
 *
 * return
 *      number of bytes needed to hold count_bits 
 */
size_t bits_to_bytes(size_t count_bits)
{
    return count_bits / CHAR_BIT + ((count_bits % CHAR_BIT == 0) ? 0 : 1);
}

/*
 * name
 *      change_endianess
 *
 * parameters
 *      l - lua interpreter state
 *      value - integer value to convert to requested endianess
 *      count_bits - number of least significant bits to use from value
 *      endianess - requested endianess
 *      result - out parameter. caries out the converted integer as buffer of bytes
 *      result_len - length of the result buffer
 *
 * return
 *      number of bytes used in result buffer
 *
 * throw
 *      internal error - result buffer is to small
 *      wrong format - little endian requested for incomplete bytes
 *                     allowing little endian for incomplete bytes would
 *                     introduce size irregularities. for example
 *                     9:int:little for 0x01ff would be ff01 which is 16:int:little
 * 
 * rationale
 *      the reason for writing this function and not using htonl when
 *      network byte order is requested is portability. the hton/ntoh 
 *      functions convert to little endian only if the platform is little
 *      endian. we could check the platform and then call the htonl function
 *      to flip endianess but it would be only more confusing.
 *      hton/ntoh functions can be used on 16 and 32 bit integers only
 */
static size_t change_endianess(
        lua_State *l,
        lua_Integer value,
        size_t count_bits,
        ELEMENT_ENDIANESS endianess, 
        unsigned char *result, 
        size_t result_len)
{
    if(result_len < count_bits / CHAR_BIT + 1)
    {
        luaL_error(l, "internal error: internal buffer error at %s:%d", __FILE__, __LINE__);
    }

    if(count_bits % CHAR_BIT !=0 && endianess == EE_LITTLE)
    {
        luaL_error(l, "wrong format: Little endian is supported for %d bit bounds", CHAR_BIT);
    }

    size_t count_bytes = bits_to_bytes(count_bits);

    int i;
    for(i = 0; i < count_bytes; ++i)
    {
        if(endianess == EE_BIG || endianess == EE_DEFAULT)
        {
            result[i] = (value >> ((count_bytes - i - 1) * CHAR_BIT)) & 0xff;
        }
        else if (endianess == EE_LITTLE)
        {
            result[i] = (value >> (i  * CHAR_BIT)) & 0xff;
        }
    }
    return count_bytes;
}

static lua_Integer clear_unused_bits(lua_Integer value, size_t used_bits)
{
    if(used_bits >= sizeof(value) * CHAR_BIT)
    {
        return value;
    }
    lua_Integer mask = (((~0) << used_bits) ^ (~0)); 
    return value & mask;
}

static void pack_int(lua_State *l, ELEMENT_DESCRIPTION *elem, lua_Integer value, PACK_STATE *state)
{
    /*
    printf("pack_int: value = 0x%08x, size = %d, endianess = %s\n",
            value, elem->size, ENDIANESSES[elem->endianess]);
    */

    size_t count_packed_bits = elem->size;
    unsigned char val_buff[sizeof(lua_Integer) + 1];
    memset(val_buff, 0, sizeof(val_buff));
    size_t count_bytes = 
        change_endianess(
                          l, 
                          clear_unused_bits(value, elem->size), 
                          elem->size, elem->endianess, 
                          val_buff, 
                          sizeof(val_buff));

    size_t bit_offset = state->current_bit % CHAR_BIT;
    size_t source_bit_offset = elem->size % CHAR_BIT;
    /* bit_gap can be CHAR_BIT */
    int bit_gap = ((int)CHAR_BIT - (int)bit_offset - (int)source_bit_offset) % CHAR_BIT; 
    unsigned char *current_byte = state->prep_buffer + state->current_bit / CHAR_BIT;

    /* flash buffer */
    if(state->current_bit + elem->size > state->result_bits)
    {
        size_t size = current_byte - state->prep_buffer;
        unsigned char tmp = *current_byte;
        luaL_addsize(state->buffer, size);

        state->prep_buffer = luaL_prepbuffer(state->buffer);
        memset(state->prep_buffer, 0, LUAL_BUFFERSIZE);
        state->current_bit = bit_offset;
        current_byte = state->prep_buffer;
        *current_byte = tmp;
    }

    if(bit_offset == 0 && source_bit_offset == 0)
    {
        /* simple copy */
        int i;
        for(i = 0; i < count_bytes; ++i)
        {
            *current_byte = val_buff[i];
            ++current_byte;
        }
    }
    else if(bit_offset == 0 && source_bit_offset != 0)
    {
        /* move left */
        int i;
        for(i = 0; i < count_bytes - 1; ++i)
        {
            *current_byte = (val_buff[i] << (CHAR_BIT - source_bit_offset)) & 0xff;
            *current_byte |= (val_buff[i + 1] >> source_bit_offset) & 0xff;
            ++current_byte;
        }
        *current_byte = (val_buff[i] << (CHAR_BIT - source_bit_offset)) & 0xff;
     }
    else if(bit_offset != 0 && source_bit_offset == 0)
    {
        /* move right */
        int i;
        unsigned char tmp = 0;
        for(i = 0; i < count_bytes; ++i)
        {
            *current_byte |= ((val_buff[i] >> bit_offset) | tmp) & 0xff;
            tmp = (val_buff[i] << (CHAR_BIT - bit_offset)) & 0xff; 
            ++current_byte;
        }
        *current_byte |= tmp;
    }
    else
    {
        if(bit_gap < 0)
        {
            bit_gap = abs(bit_gap);
            /* move right */
            int i;
            unsigned char tmp = 0;
            for(i = 0; i < count_bytes; ++i)
            {
                *current_byte |= ((val_buff[i] >> bit_gap) | tmp) & 0xff;
                tmp = (val_buff[i] << (CHAR_BIT - bit_gap)) & 0xff; 
                ++current_byte;
            }
            *current_byte |= tmp & 0xff; 
        }
        else if(bit_gap > 0)
        {
            /* move left */
            int i;
            for(i = 0; i < count_bytes - 1; ++i)
            {
                *current_byte |= (val_buff[i] << bit_gap) & 0xff;
                *current_byte |= (val_buff[i + 1] >> (CHAR_BIT - bit_gap)) & 0xff;
                ++current_byte;
            }
            *current_byte |= (val_buff[i] << bit_gap) & 0xff;
        }
        else
        {
            int i;
            for(i = 0; i < count_bytes; ++i)
            {
                *current_byte |= val_buff[i];
                ++current_byte;
            }
        }
    }


    state->current_bit += count_packed_bits;
}

static void pack_bin(lua_State *l, ELEMENT_DESCRIPTION *elem, const unsigned char *bin, size_t len, PACK_STATE *state)
{
    size_t i;
    for(i = 0; i < len; ++i)
    {
        ELEMENT_DESCRIPTION elem;
        elem.size = CHAR_BIT;
        elem.endianess = EE_BIG;
        elem.type = ET_INTEGER; 
        pack_int(l, &elem, bin[i], state);
    }
}

static void pack_elem(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, void *arg)
{
    /*
    printf("pack_elem: %d:%s:%s at %d\n", 
    elem->size, TYPES[elem->type], ENDIANESSES[elem->endianess], arg_index);
    */

    if(elem->size == 0)
    {
        luaL_error(l, "size error: argument %d", arg_index);
    }

    PACK_STATE *state = (PACK_STATE *)arg;
    if(elem->type == ET_INTEGER)
    {
        lua_Integer value = luaL_checkinteger(l, arg_index);
        if(elem->size > sizeof(lua_Integer) * CHAR_BIT)
        {
            luaL_error(l, "size error: argument %d size (%d bits) exceeds the lua_Integer size (%d bits)", 
                    arg_index, elem->size, sizeof(lua_Integer) * CHAR_BIT);
        }
        pack_int(l, elem, value, state);
    }
    else if(elem->type == ET_BINARY)
    {
        size_t len = 0;
        const unsigned char *bin = luaL_checklstring(l, arg_index, &len);
        if(elem->size != ALL)
        {
            if(elem->size > len)
            {
                luaL_error(l, "size error: argument %d size (%d bytes) exceeds the length of input stirng (%d bytes)", 
                        arg_index, elem->size, len);
            }
            len = elem->size;
        }
        pack_bin(l, elem, bin, len, state);
    }
    else
    {
        luaL_error(l, "wrong format: unexpected type %d", elem->type);
    }
}

static lua_Integer toint(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, const unsigned char *buffer, size_t buffer_len)
{
    if(elem->size % CHAR_BIT != 0 && elem->endianess == EE_LITTLE)
    {
        luaL_error(l, "argument %d: little endianess supported for %d bit bounds", arg_index, CHAR_BIT);
    }

    lua_Integer result = 0;
    if(elem->endianess == EE_BIG || elem->endianess == EE_DEFAULT)
    {
        int i;
        for(i = buffer_len - 1; i >= 0; --i)
        {
            size_t shift = (buffer_len - i - 1) * CHAR_BIT;
            result |= buffer[i] << shift;
        }
    }
    else if(elem->endianess == EE_LITTLE)
    {
        int i;
        for(i = 0; i < buffer_len; ++i)
        {
            size_t shift = i * CHAR_BIT;
            result |= buffer[i] << shift; 
        }
    }
    else
    {
        luaL_error(l, "unsupported endianess %d", elem->endianess);
    }
    return result;
}

static lua_Integer unpack_int_no_push(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, UNPACK_STATE *state)
{
    /*
     * printf("unpack_int: %d:%s:%s\n", elem->size, TYPES[elem->type], ENDIANESSES[elem->endianess]);
     */
    /*
     * check space
     */
    if(state->source_bits < elem->size)
    {
        luaL_error(l, "size error: element %d size (%d bits) exceeds the size of input reminder (%d bits)",
                arg_index, elem->size, state->source_bits);
    }

    if(elem->size > sizeof(lua_Integer) * CHAR_BIT)
    {
        luaL_error(l, "size error: argument %d size (%d bits) exceeds the lua_Integer size (%d bits)", 
                arg_index, elem->size, sizeof(lua_Integer) * CHAR_BIT);
    }

    if(elem->size == 0)
    {
        luaL_error(l, "size error: argument %d size must be greater then 0 bits", arg_index);
    }


    size_t bit_offset = state->current_bit % CHAR_BIT;
    size_t result_bit_offset = elem->size % CHAR_BIT;
    const unsigned char *current_byte = state->source + state->current_bit / CHAR_BIT;

    lua_Integer result = 0xdeadbeef;

    if(bit_offset == 0 && result_bit_offset == 0)
    {
        size_t bytes_to_copy = elem->size / CHAR_BIT;
        unsigned char result_buffer[bytes_to_copy];
        memcpy(result_buffer, current_byte, bytes_to_copy);
        result = toint(l, elem, arg_index, result_buffer, sizeof(result_buffer)); 
    }
    else
    {
        /* check for source end */
        size_t end_bit = state->current_bit + elem->size;
        const unsigned char *end_byte = state->source + end_bit / CHAR_BIT;

        size_t bytes_to_copy = bits_to_bytes(elem->size);
        unsigned char result_buffer[bytes_to_copy];
        memset(result_buffer, 0, bytes_to_copy);

        size_t right_shift = CHAR_BIT - end_bit % CHAR_BIT;
        size_t left_shift = CHAR_BIT - right_shift;
        int i;
        for(i = bytes_to_copy - 1; i >= 0 && end_byte > current_byte; --i)
        {
            result_buffer[i] = (*end_byte >> right_shift) & 0xff; 
            --end_byte;
            result_buffer[i] |= (*end_byte << left_shift) & 0xff;
        }
        if(i >= 0)
        {
            result_buffer[0] |= (*end_byte >> right_shift) & 0xff; 
        }

        result = toint(l, elem, arg_index, result_buffer, sizeof(result_buffer)); 
        result = clear_unused_bits(result, elem->size);
    }
    state->current_bit += elem->size;
    state->source_bits -= elem->size;

    return result;
}

static void unpack_int(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, UNPACK_STATE *state)
{
    lua_Integer result = unpack_int_no_push(l, elem, arg_index, state);
    ++state->return_count;
    lua_pushinteger(l, result);
}

static void unpack_bin(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, UNPACK_STATE *state)
{
    if(elem->size == REST)
    {
        if(state->current_bit % CHAR_BIT != 0)
        {
            luaL_error(l, "wrong format: using rest length specifier for incomplete octets at element %d", arg_index);
        }

        elem->size = (state->source_end - state->source) - state->current_bit / CHAR_BIT;
    }

    printf("elem->size %d, state->source_bits / CHAR_BIT %d\n", elem->size , state->source_bits / CHAR_BIT);

    if(elem->size > state->source_bits / CHAR_BIT)
    {
        luaL_error(l, "size error: requested length for element %d is greater then remaining part of input", arg_index);
    }

    luaL_Buffer b; 
    luaL_buffinit(l, &b);

    size_t i = 0;
    while(i < elem->size)
    {
        unsigned char *result = luaL_prepbuffer(&b);
        size_t j = 0;
        while(i < elem->size && j < LUAL_BUFFERSIZE)
        {
            ELEMENT_DESCRIPTION tmp_elem;
            tmp_elem.size = CHAR_BIT;
            tmp_elem.endianess = EE_BIG;
            tmp_elem.type = ET_INTEGER; 
            result[j] = unpack_int_no_push(l, &tmp_elem, arg_index, state);
            ++i;
            ++j;
        }
        luaL_addsize(&b, j);
    }
    ++state->return_count;
    luaL_pushresult(&b);
}

static void unpack_elem(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, void *arg)
{
    UNPACK_STATE *state = (UNPACK_STATE *)arg;
    if(elem->type == ET_INTEGER)
    {
        unpack_int(l, elem, arg_index, state);
    }
    else if(elem->type == ET_BINARY)
    {
        unpack_bin(l, elem, arg_index, state);
    }
    else
    {
        luaL_error(l, "unexpected type %d", elem->type);
    }
}

static void throw_wrong_len(lua_State *l, size_t len, size_t max_len)
{
    if(len > max_len)
    {
        luaL_error(l, "token len longer then %d bytes", max_len);
    }
}

int compare_token(const char *keyword, const char *token, size_t len)
{
    if(strlen(keyword) == len && memcmp(keyword, token, len) == 0)
    {
        return 1;
    }
    return 0;
}

static ELEMENT_TYPE totype(lua_State *l, const char *token, size_t token_len)
{
    int i = 1;
    while(TYPES[i])
    {
        if(compare_token(TYPES[i], token, token_len))
        {
            return i;
        } 
        ++i;
    }
    return luaL_error(l, "wrong format: unexpected type token (%s)", token); 
}

static ELEMENT_ENDIANESS toendianess(lua_State *l, const char *token, size_t token_len)
{
    int i = 1;
    while(ENDIANESSES[i])
    {
        if(compare_token(ENDIANESSES[i], token, token_len))
        {
            return i;
        } 
        ++i;
    }
    return luaL_error(l, "wrong format: unexpected endianess token (%s)", token); 
}

static size_t tosize(lua_State *l, const char *token, size_t token_len)
{
    size_t size = -1;
    if(compare_token(ALL_SPECIFIER, token, token_len))
    {
        size = ALL;
    }
    else if(compare_token(REST_SPECIFIER, token, token_len))
    {
        size = REST;
    }
    else
    {
        // strtol
        size = atoi(token);
    }
    return size;
}

static void parse(lua_State *l, ELEM_HANDLER handler, void *arg)
{
    size_t len = 0;
    const char *format = luaL_checklstring(l, 1, &len);

    const char *token = format;
    size_t token_len = 0;
    ELEMENT_DESCRIPTION elem;
    memset(&elem, 0, sizeof(elem));
    PARSE_STATE state = SIZE_STATE;
    int argnum = 2;
    int i = 0;
    while(i < len)
    {
        switch(state)
        {
            case SIZE_STATE:
                if(format[i] == PART_DELIMITER && token_len > 0)
                {
                    state = TYPE_STATE;
                    elem.size = tosize(l, token, token_len);
                    token = token + token_len + 1;
                    token_len = 0;
                }
                else if(!isalnum(format[i]))
                {
                    luaL_error(l, "wrong format: not a digit (%c at %d) where digit is expected", 
                            format[i], i + 1);
                }
                else
                {
                    ++token_len;
                }
                break;

            case TYPE_STATE:
                if(format[i] == PART_DELIMITER && token_len > 0)
                {
                    state = ENDIANESS_STATE;
                    elem.type = totype(l, token, token_len);
                    token = token + token_len + 1;
                    token_len = 0;
                }
                else if(strchr(ELEMENT_DELIMITERS, format[i]))
                {
                    state = SPACE_STATE;
                    elem.type = totype(l, token, token_len);
                    token = token + token_len + 1;
                    token_len = 0;
                    handler(l, &elem, argnum, arg); 
                    ++argnum;
                    memset(&elem, 0, sizeof(elem));
                }
                else if(!isalpha(format[i]))
                {
                    luaL_error(l, "not a letter (%c at %d) where letter is expected", 
                            format[i], i + 1);
                }
                else
                {
                    ++token_len;
                }
                break;

            case ENDIANESS_STATE:
                if(strchr(ELEMENT_DELIMITERS, format[i]))
                {
                    state = SPACE_STATE;
                    elem.endianess = toendianess(l, token, token_len);
                    token = token + token_len + 1;
                    token_len = 0;
                    handler(l, &elem, argnum, arg); 
                    ++argnum;
                    memset(&elem, 0, sizeof(elem));
                }
                else if(!isalpha(format[i]))
                {
                    luaL_error(l, "not a letter (%c at %d) where letter is expected", 
                            format[i], i + 1);
                }
                else
                {
                    ++token_len;
                }
                break;

            case SPACE_STATE:
                if(!strchr(ELEMENT_DELIMITERS, format[i]))
                {
                    state = SIZE_STATE;
                    token_len = 0;
                    --i;
                }
                else
                {
                    ++token;
                }
                break;

            default:
                luaL_error(l, "unexpected state (%02x at %d)", format[i], i + 1);
                break;
        }
        ++i;
    }

    switch(state)
    {
        case SIZE_STATE:
            luaL_error(l, "wrong format: incomplete format string %s", format); 
            break;

        case TYPE_STATE:
            {
                elem.type = totype(l, token, token_len);
                handler(l, &elem, argnum, arg); 
            }
            break;

        case ENDIANESS_STATE:
            {
                elem.endianess = toendianess(l, token, token_len);
                handler(l, &elem, argnum, arg); 
            }

        case SPACE_STATE:
            break;

        default:
            luaL_error(l, "unexpected state at %d", i + 1);
            break;
    }
}

static int l_pack(lua_State *l)
{
    luaL_Buffer b; 
    luaL_buffinit(l, &b);

    PACK_STATE state;
    state.buffer = &b;
    state.prep_buffer = luaL_prepbuffer(&b);
    memset(state.prep_buffer, 0, LUAL_BUFFERSIZE);
    state.current_bit = 0;
    state.result_bits = LUAL_BUFFERSIZE * CHAR_BIT;

    parse(l, pack_elem, (void *)&state);
    luaL_addsize(&b, state.current_bit / CHAR_BIT);
    luaL_pushresult(&b);
    return 1;
}

static int l_unpack(lua_State *l)
{
    /*
     * check two and only two paramenters
     * count bits in input string
     */

    size_t source_len = 0;
    const unsigned char *source = luaL_checklstring(l, 2, &source_len); 
    UNPACK_STATE state;
    state.return_count = 0;
    state.current_bit = 0;
    state.source_bits = source_len * CHAR_BIT;
    state.source = source;
    state.source_end = source + source_len;
    parse(l, unpack_elem, (void *)&state);
    return state.return_count;
}

static const struct luaL_reg bitstring [] = 
{
    {"pack", l_pack},
    {"unpack", l_unpack},
    {NULL, NULL}  /* sentinel */
};

int luaopen_bitstring(lua_State *l) 
{
    luaL_openlib(l, "bitstring", bitstring, 0);
    return 1;
}


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
 * description
 *      calculate the number of bytes for given number of bits
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
 * description
 *      change endianess of input and return as array of bytes
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

/*
 * name
 *      clear_unused_bits
 *
 * description
 *      clear unused bits in input value
 *
 * paramenters
 *      value - input from which unused bits will be cleared
 *      used_bits - number of bits that are in use
 *
 * returns
 *      the input value with all unused bits set to zero
 */
static lua_Integer clear_unused_bits(lua_Integer value, size_t used_bits)
{
    if(used_bits >= sizeof(value) * CHAR_BIT)
    {
        return value;
    }
    lua_Integer mask = (((~0) << used_bits) ^ (~0)); 
    return value & mask;
}

/*
 * name
 *      pack_int
 *
 * description
 *      pack integer into result buffer
 *
 * paramenters
 *      l - lua state
 *      elem - element description
 *      value - value to pack
 *      state - pack state and intermediate results that are passed between
 *              invocations
 *
 * returns
 *      the function collects the result in buffer member of state parameter
 *
 * rationale
 *      The function splits handling into different cases of bit alignment in
 *      input and in result buffer. see comments in the body of the function
 */
static void pack_int(lua_State *l, ELEMENT_DESCRIPTION *elem, lua_Integer value, PACK_STATE *state)
{
    size_t count_packed_bits = elem->size;
    unsigned char val_buff[sizeof(lua_Integer) + 1];
    /* start clean */
    memset(val_buff, 0, sizeof(val_buff));
    /* change the endianess and explicitely represent the value as buffer of bytes */
    size_t count_bytes = 
        change_endianess(
                          l, 
                          clear_unused_bits(value, elem->size), 
                          elem->size, elem->endianess, 
                          val_buff, 
                          sizeof(val_buff));

    /* number of least significant bits in the result buffer that are over byte bounds */
    size_t bit_offset = state->current_bit % CHAR_BIT;
    /* number of most significant bits in the input value that are over byte bounds */
    size_t source_bit_offset = elem->size % CHAR_BIT;
    /* bit_gap can be CHAR_BIT */
    /* if bit_offset would be assigned to a byte and ORed with source_bit_offset */
    /* there would be some bits inside the byte that wouldn't be set, or would overlap */
    /* or it would be a perfect match. this is the bit_gap */
    int bit_gap = ((int)CHAR_BIT - (int)bit_offset - (int)source_bit_offset) % CHAR_BIT; 

    /* current byte in the result buffer */
    unsigned char *current_byte = state->prep_buffer + state->current_bit / CHAR_BIT;

    /* whenever the temporary prep_buffer is full flush it into result buffer */
    if(state->current_bit + elem->size > state->result_bits)
    {
        size_t size = current_byte - state->prep_buffer;
        /* save the unfinished byte for next prep_buffer */
        unsigned char tmp = *current_byte;
        luaL_addsize(state->buffer, size);

        state->prep_buffer = luaL_prepbuffer(state->buffer);
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
            *current_byte = 0;
        }
    }
    else if(bit_offset == 0 && source_bit_offset != 0)
    {
        /* result string u - used bits*/
        /* uuuu uuuu  uuuu uuuu  uuuu uuuu  0000 0000  0000 0000 */
        /* input value in a char buffer */
        /* 0000 0uuu uuuu uuuu */ 
        /* moved left and ORed with result buffer */
        /* uuuu uuuu uuu0 0000 */ 
         
        int i;
        for(i = 0; i < count_bytes - 1; ++i)
        {
            *current_byte = (val_buff[i] << (CHAR_BIT - source_bit_offset)) & 0xff;
            *current_byte |= (val_buff[i + 1] >> source_bit_offset) & 0xff;
            ++current_byte;
            *current_byte = 0;
        }
        *current_byte = (val_buff[i] << (CHAR_BIT - source_bit_offset)) & 0xff;
    }
    else if(bit_offset != 0 && source_bit_offset == 0)
    {
        /* result string u - used bits*/
        /* uuuu uuuu  uuuu uuuu  uuuu uuuu  uuu0 0000  0000 0000 */
        /* input value in a char buffer */
        /* uuuu uuuu  uuuu uuuu */ 
        /* moved right and ORed with result buffer */
        /* 000u uuuu uuuu  uuuu uuu0 0000 */ 
         int i;
        unsigned char tmp = 0;
        for(i = 0; i < count_bytes; ++i)
        {
            *current_byte |= ((val_buff[i] >> bit_offset) | tmp) & 0xff;
            tmp = (val_buff[i] << (CHAR_BIT - bit_offset)) & 0xff; 
            ++current_byte;
            *current_byte = 0;
        }
        *current_byte |= tmp;
    }
    else
    {
        if(bit_gap < 0)
        {
            /* result string u - used bits*/
            /* uuuu uuuu  uuuu uuuu  uuuu uuuu  uuu0 0000  0000 0000 */
            /* input value in a char buffer */
            /* 0uuu uuuu  uuuu uuuu */ 

            bit_gap = abs(bit_gap);
            /* move right */
            int i;
            unsigned char tmp = 0;
            for(i = 0; i < count_bytes; ++i)
            {
                *current_byte |= ((val_buff[i] >> bit_gap) | tmp) & 0xff;
                tmp = (val_buff[i] << (CHAR_BIT - bit_gap)) & 0xff; 
                ++current_byte;
                *current_byte = 0;
            }
            *current_byte |= tmp & 0xff; 
        }
        else if(bit_gap > 0)
        {
            /* result string u - used bits*/
            /* uuuu uuuu  uuuu uuuu  uuuu uuuu  uuu0 0000  0000 0000 */
            /* input value in a char buffer */
            /* 0000 uuuu  uuuu uuuu */ 

            int i;
            for(i = 0; i < count_bytes - 1; ++i)
            {
                *current_byte |= (val_buff[i] << bit_gap) & 0xff;
                *current_byte |= (val_buff[i + 1] >> (CHAR_BIT - bit_gap)) & 0xff;
                ++current_byte;
                *current_byte = 0;
            }
            *current_byte |= (val_buff[i] << bit_gap) & 0xff;
        }
        else
        {
            /* result string u - used bits*/
            /* uuuu uuuu  uuuu uuuu  uuuu uuuu  uuu0 0000  0000 0000 */
            /* input value in a char buffer */
            /* 000u uuuu  uuuu uuuu */ 

            int i;
            for(i = 0; i < count_bytes; ++i)
            {
                *current_byte |= val_buff[i];
                ++current_byte;
                *current_byte = 0;
            }
        }
    }
    state->current_bit += count_packed_bits;
}

/*
 * name
 *      pack_bin
 *
 * description
 *      pack binary into result buffer
 *
 * paramenters
 *      l - lua state
 *      elem - element description
 *      bin - input binary string
 *      len - input length
 *      state - pack state and intermediate results that are passed between
 *              invocations
 *
 * returns
 *      the function collects the result in buffer member of state parameter
 *
 * rationale
 *      The function handles the binary string as sequence of integers
 *      to allow packing strings of arbitrary length on arbitrary bit
 *      positions.
 *
 * future work
 *      optimize with memcpy for the most common usage pattern
 */
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

/*
 * name
 *      pack_elem
 *
 * description
 *      pack element into result buffer
 *
 * paramenters
 *      l - lua state
 *      elem - element description
 *      arg_index - number of element in format string. starts from 1 
 *      arg - pack state and intermediate results that are passed between
 *              invocations
 *
 * returns
 *      the function collects the result in buffer member of state parameter
 *
 * throws
 *      size error - element size is zero
 *      size error - element size exceeds lua_Integer size
 *      size error - element size exceeds input size for binary strings
 *      wrong format - unknown input type
 *
 * future work
 *      add more types - floating point, tables, etc
 */
static void pack_elem(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, void *arg)
{
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

/*
 * name
 *      toint
 *
 * description
 *      convert array of bytes to int taking into account the endianess
 *      of the array
 *
 * paramenters
 *      l - lua state
 *      elem - element description
 *      arg_index - number of element in format string. starts from 1 
 *      buffer - the buffer to convert
 *      buffer_len - length of input buffer
 *
 * returns
 *      the integer in host byte order
 *
 * throws
 *      wrong format - little endianess requested for size % CHAR_BIT != 0
 *      size error - element size is zero
 *      size error - element size exceeds lua_Integer size
 *      size error - element size exceeds input size for binary strings
 *      wrong format - unknown input type
 *
 * rationale
 *      this function was implemented rather then just casting to int and moving bits
 *      to allow handling of integers packed on arbitrary bit bounds
 *      for example 32 bit integer may require 5 bytes
 *      0000 uuuu  uuuu uuuu  uuuu uuuu  uuuu uuuu  uuuu 0000   
 *
 * future work
 *      optimize for most common usage pattern
 */
static lua_Integer toint(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, const unsigned char *buffer, size_t buffer_len)
{
    if(elem->size % CHAR_BIT != 0 && elem->endianess == EE_LITTLE)
    {
        luaL_error(l, "wrong format: argument %d: little endianess supported for %d bit bounds only", arg_index, CHAR_BIT);
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
        luaL_error(l, "wrong format: unsupported endianess %d", elem->endianess);
    }
    return result;
}

/*
 * name
 *      unpack_int_no_push 
 *
 * description
 *      unpack integer from input buffer without pushing it onto lua
 *      stack
 *
 * paramenters
 *      l - lua state
 *      elem - element description
 *      arg_index - number of element in format string. starts from 1 
 *      state - unpack state passed between invocations
 *
 * returns
 *      the unpacked integer in host byte order
 *
 * throws
 *      size error - element size exceeds the size of input reminder
 *      size error - element size exceeds the size of lua_Integer
 *      size error - element size is zero
 *
 * rationale
 *      handling different usage patterns separately provides opportunities
 *      for optimization
 *
 * future work
 *      optimize for most common usage pattern
 */
static lua_Integer unpack_int_no_push(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, UNPACK_STATE *state)
{
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


    /* number of most significant unprossed bits in the input buffer that are over byte bounds */
    size_t bit_offset = state->current_bit % CHAR_BIT;
    /* number of most significant bits in the result value that are over byte bounds */
    size_t result_bit_offset = elem->size % CHAR_BIT;
    /* current byte in the input buffer */
    const unsigned char *current_byte = state->source + state->current_bit / CHAR_BIT;

    lua_Integer result = 0xdeadbeef;

    if(bit_offset == 0 && result_bit_offset == 0)
    {
        /* input buffer p - processed bits, u - unprocessed bits*/
        /* pppp pppp  pppp pppp uuuu uuuu uuuu uuuu */
        size_t bytes_to_copy = elem->size / CHAR_BIT;
        unsigned char result_buffer[bytes_to_copy];
        memcpy(result_buffer, current_byte, bytes_to_copy);
        result = toint(l, elem, arg_index, result_buffer, sizeof(result_buffer)); 
    }
    /*
     * check optimization opportunities
    else if(bit_offset != 0 && result_bit_offset == 0)
    {
    }
    else if(bit_offset == 0 && result_bit_offset != 0)
    {
    }
    */
    else
    {
        /* check for source end */
        size_t end_bit = state->current_bit + elem->size;
        const unsigned char *end_byte = state->source + end_bit / CHAR_BIT;

        size_t bytes_to_copy = bits_to_bytes(elem->size);
        unsigned char result_buffer[bytes_to_copy];
        memset(result_buffer, 0, bytes_to_copy);

        /* copy bits to result_buffer while adjusting on byte bounds */
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
            /* there are still some bits to copy from end_byte */
            result_buffer[0] |= (*end_byte >> right_shift) & 0xff; 
        }

        result = toint(l, elem, arg_index, result_buffer, sizeof(result_buffer)); 
        result = clear_unused_bits(result, elem->size);
    }
    state->current_bit += elem->size;
    state->source_bits -= elem->size;
    return result;
}

/*
 * name
 *      unpack_int
 *
 * description
 *      unpack integer from input buffer and push it onto lua stack. 
 *      update the number of return values
 *
 * paramenters
 *      l - lua state
 *      elem - element description
 *      arg_index - number of element in format string. starts from 1 
 *      state - unpack state passed between invocations
 *
 */
static void unpack_int(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, UNPACK_STATE *state)
{
    lua_Integer result = unpack_int_no_push(l, elem, arg_index, state);
    ++state->return_count;
    lua_pushinteger(l, result);
}

/*
 * name
 *      unpack_bin
 *
 * description
 *      unpack binary string from input buffer and push it onto lua stack. 
 *      update the number of return values
 *
 * paramenters
 *      l - lua state
 *      elem - element description
 *      arg_index - number of element in format string. starts from 1 
 *      state - unpack state passed between invocations
 *
 * throws
 *      wrong format - using rest length specifier for incomplete bytes
 *      size error - requested length is greater then remaining part of input
 *
 * rationale
 *      the function unpacks stream of bytes as array of integers 
 *      packed at arbitrary bit positions.
 *
 * future work
 *      optimize for most common usage pattern
 */
static void unpack_bin(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, UNPACK_STATE *state)
{
    if(elem->size == REST)
    {
        if(state->current_bit % CHAR_BIT != 0)
        {
            luaL_error(l, "wrong format: using rest length specifier for incomplete bytes at element %d", arg_index);
        }

        elem->size = (state->source_end - state->source) - state->current_bit / CHAR_BIT;
    }

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

/*
 * name
 *      unpack_elem
 *
 * description
 *      unpack element from input buffer 
 *
 * paramenters
 *      l - lua state
 *      elem - element description
 *      arg_index - number of element in format string. starts from 1 
 *      state - unpack state passed between invocations
 *
 * throws
 *      wrong format - unexpected type
 *
 * future work
 *      add more types
 */
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
        luaL_error(l, "wrong format: unexpected type %d", elem->type);
    }
}

/*
 * name
 *      compare_token
 *
 * description
 *      compare zero terminated keyword with token that is
 *      not zero terminated
 *
 * paramenters
 *      keyword - zero terminated keyword
 *      token - token obtained from parsing the format string
 *      len - length of the token
 */
int compare_token(const char *keyword, const char *token, size_t len)
{
    if(strlen(keyword) == len && memcmp(keyword, token, len) == 0)
    {
        return 1;
    }
    return 0;
}

/*
 * name
 *      totype
 *
 * description
 *      convert type token to type enum value
 *
 * paramenters
 *      l - lua state
 *      token - token obtained from parsing the format string
 *      token_len - length of the token
 */
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

/*
 * name
 *      toendianess
 *
 * description
 *      convert endianess token to endianess enum value
 *
 * paramenters
 *      l - lua state
 *      token - token obtained from parsing the format string
 *      token_len - length of the token
 */
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

/*
 * name
 *      tosize
 *
 * description
 *      convert size token to size value
 *
 * paramenters
 *      l - lua state
 *      token - token obtained from parsing the format string
 *      token_len - length of the token
 */
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

/*
 * name
 *      parse
 *
 * description
 *      parse format string and call handler function for
 *      each parsed element. 
 *      function implemented as a simple state machine
 *      with following states
 *          SIZE_STATE - parsing size part of the element
 *          TYPE_STATE - parsing type part of the element
 *          ENDIANESS_STATE - parsing optional endianess part of the element
 *          SPACE_STATE - parsing delimiter between elements
 *      state transitions are 
 *      SIZE_STATE -> TYPE_STATE -> ENDIANESS_STATE -> SPACE_STATE -> SIZE_STATE
 *      SIZE_STATE -> TYPE_STATE -> SPACE_STATE -> SIZE_STATE
 *      SIZE_STATE -> TYPE_STATE -> ENDIANESS_STATE -> SPACE_STATE -> END
 *      SIZE_STATE -> TYPE_STATE -> SPACE_STATE -> END
 *      SIZE_STATE -> TYPE_STATE -> ENDIANESS_STATE -> END
 *      SIZE_STATE -> TYPE_STATE -> END
 *
 * paramenters
 *      l - lua state
 *      handler - callback to handle the element
 *      arg - opaque parameter that is passed to handler between invocations
 *
 * rationale
 *      the function separates parsing logic from logic that moves bits
 *      both may evolve independently
 *
 * future work
 *      check for performance
 *      rewrite better - less code, faster
 */
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
                    luaL_error(l, "wrong format: not a letter (%c at %d) where letter is expected", 
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
                    luaL_error(l, "wrong format: not a letter (%c at %d) where letter is expected", 
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
                luaL_error(l, "wrong format: unexpected state (%02x at %d)", format[i], i + 1);
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

/*
 * name
 *      l_pack
 *
 * description
 *      lua_CFunction for packing
 *
 * paramenters
 *      l - lua state
 *
 * returns
 *      pushes the result string onto lua stack and
 *      returns 1
 */
static int l_pack(lua_State *l)
{
    luaL_Buffer b; 
    luaL_buffinit(l, &b);

    PACK_STATE state;
    state.buffer = &b;
    state.prep_buffer = luaL_prepbuffer(&b);
    state.current_bit = 0;
    state.result_bits = LUAL_BUFFERSIZE * CHAR_BIT;

    parse(l, pack_elem, (void *)&state);
    luaL_addsize(&b, state.current_bit / CHAR_BIT);
    luaL_pushresult(&b);
    return 1;
}

/*
 * name
 *      l_unpack
 *
 * description
 *      lua_CFunction for unpacking
 *
 * paramenters
 *      l - lua state
 *
 * returns
 *      number of return values
 */
static int l_unpack(lua_State *l)
{
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

#include "bitstring/lhexdump.c"

static const struct luaL_reg bitstring [] = 
{
    {"pack", l_pack},
    {"unpack", l_unpack},
    {"hexdump", l_hexdump},
    {NULL, NULL}  /* sentinel */
};

/*
 * name
 *      luaopen_bitstring
 *
 * description
 *      the only function that is exported by the library.
 *      registers lua_CFunctions implemented in the library
 *
 * paramenters
 *      l - lua state
 *
 * returns
 *      number of return values
 */
int luaopen_bitstring(lua_State *l) 
{
    luaL_openlib(l, "bitstring", bitstring, 0);
    return 1;
}


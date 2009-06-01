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

#ifndef WIN32
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <ctype.h>
#include <errno.h>

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
#include <lua.h>
#include <lauxlib.h>
#ifdef __cplusplus
}
#endif // __cplusplus


#include <stdint.h>



#ifdef WIN32
#pragma warning(disable : 4996)
#endif // WIN32
/*
 * Element types
 */
typedef enum
{
    ET_UNDEFINED = 0,
    /* integer of up to sizeof(lua_Integer) * CHAR_BIT bits */
    ET_INTEGER,
    /* octet string. each octet may hold values of 0-255 */
    ET_BINARY,
    /* floating point number of up to sizeof(lua_Number) * CHAR_BIT bits */
    ET_FLOAT,
} ELEMENT_TYPE;

/* 
 * type tokens 
 */
static const char *TYPES[] =
{
    "undefined",
    "int",
    "bin",
    "float",
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
 * bitmatch userdata
 */
typedef struct
{
    /* count of elements in array. */
    /* this value is set in the end of compiling */
    size_t element_count;
    /* start of array of elements */
    ELEMENT_DESCRIPTION elements[1];
} BITMATCH;

/*
 * compile state passed between handler function invocations */
typedef struct
{
    /* current element */
    size_t current;
    /* total element count in bitmatch array */
    size_t element_count;
    /* the object that will be returned from compile function */
    BITMATCH *bitmatch;
} COMPILE_STATE;


/*
 * pointer to callback function that process elements
 */
typedef void (*ELEM_HANDLER)(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, void *arg);

static BITMATCH *get_bitmatch(lua_State *l, int index);

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

    size_t i;
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
 *      basic_pack_int
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
static void basic_pack_int(lua_State *l, ELEMENT_DESCRIPTION *elem, lua_Integer value, PACK_STATE *state)
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
    /* leave one spare byte for code that zeroes a byte ahead */
    if(state->current_bit + elem->size >= state->result_bits)
    {
        size_t size = current_byte - state->prep_buffer;
        /* save the unfinished byte for next prep_buffer */
        unsigned char tmp = *current_byte;
        luaL_addsize(state->buffer, size);

        state->prep_buffer = (unsigned char *)luaL_prepbuffer(state->buffer);
        state->current_bit = bit_offset;
        current_byte = state->prep_buffer;
        *current_byte = tmp;
    }

    if(bit_offset == 0 && source_bit_offset == 0)
    {
        /* simple copy */
        size_t i;
        for(i = 0; i < count_bytes; ++i)
        {
            *current_byte = val_buff[i];
            ++current_byte;
        }
        *current_byte = 0;
    }
    else if(bit_offset == 0 && source_bit_offset != 0)
    {
        /* result string u - used bits*/
        /* uuuu uuuu  uuuu uuuu  uuuu uuuu  0000 0000  0000 0000 */
        /* input value in a char buffer */
        /* 0000 0uuu uuuu uuuu */ 
        /* moved left and ORed with result buffer */
        /* uuuu uuuu uuu0 0000 */ 
         
        size_t i;
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
        size_t i;
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
            size_t i;
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

            size_t i;
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

            size_t i;
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
 *      pack_int
 *
 * description
 *      validate size and call basic_pack_int
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
 * throws
 *      size error - element size exceeds lua_Integer size
 */
static void pack_int(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, PACK_STATE *state)
{
    lua_Integer value = luaL_checkinteger(l, arg_index);
    if(elem->size > sizeof(lua_Integer) * CHAR_BIT)
    {
        luaL_error(l, 
                "size error: argument %d size (%d bits) exceeds the lua_Integer size (%d bits)", 
                arg_index, elem->size, sizeof(lua_Integer) * CHAR_BIT);
    }
    basic_pack_int(l, elem, value, state);
}

/*
 * name
 *      pack_aligned_bin
 *
 * description
 *      optimize packing of binary strings in well aligned locations
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
 *      optimize for many common use cases where binary strings are 
 *      packed on byte bounds
 */
static void pack_aligned_bin(lua_State *l, ELEMENT_DESCRIPTION *elem, const unsigned char *bin, size_t len, PACK_STATE *state)
{
    size_t reminder = len;
    unsigned char *current_byte = state->prep_buffer + state->current_bit / CHAR_BIT;
    size_t space = state->prep_buffer + LUAL_BUFFERSIZE - current_byte;
    if(reminder <= space)
    {
        memcpy(current_byte, bin, reminder);
        state->current_bit += len * CHAR_BIT;
    }
    else
    {
        while(reminder > 0)
        {
            size_t size = current_byte - state->prep_buffer;
            luaL_addsize(state->buffer, size);

            state->prep_buffer = (unsigned char *)luaL_prepbuffer(state->buffer);
            current_byte = state->prep_buffer;
            size_t space = reminder <= LUAL_BUFFERSIZE ? reminder : LUAL_BUFFERSIZE;
            memcpy(current_byte, bin, space);
            reminder -= space;
            state->current_bit = space * CHAR_BIT;
        }
    }
}

/*
 * name
 *      basic_pack_bin
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
 */
static void basic_pack_bin(lua_State *l, ELEMENT_DESCRIPTION *elem, const unsigned char *bin, size_t len, PACK_STATE *state)
{
    if(state->current_bit % CHAR_BIT == 0)
    {
        pack_aligned_bin(l, elem, bin, len, state);
    }
    else
    {
        size_t i;
        for(i = 0; i < len; ++i)
        {
            ELEMENT_DESCRIPTION elem;
            elem.size = CHAR_BIT;
            elem.endianess = EE_BIG;
            elem.type = ET_INTEGER; 
            basic_pack_int(l, &elem, bin[i], state);
        }
    }
}

/*
 * name
 *      pack_bin
 *
 * description
 *      validate size and call basic_pack_bin
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
 * throws
 *      size error - element size exceeds input size for binary strings
 */
static void pack_bin(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, PACK_STATE *state)
{
    size_t len = 0;
    const unsigned char *bin = (const unsigned char *)luaL_checklstring(l, arg_index, &len);
    if(elem->size != ALL)
    {
        if(elem->size > len)
        {
            luaL_error(l, 
                    "size error: argument %d size (%d bytes) exceeds the length of input stirng (%d bytes)", 
                    arg_index, elem->size, len);
        }
        len = elem->size;
    }

    basic_pack_bin(l, elem, bin, len, state);
}

/*
 * name
 *      pack_float
 *
 * description
 *      pack floating point number into result buffer
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
 *      size error - element size is greater then sizeof lua_Number
 *      size error - unsupported element size. currently only single (32 bit) 
 *                   and double (64 bit) precision is supported
 *
 * future work
 *      add half precision and other representation formats if somebody will
 *      find it useful
 */
static void pack_float(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, PACK_STATE *state)
{
    lua_Number value = luaL_checknumber(l, arg_index);
    if(elem->size > sizeof(lua_Number) * CHAR_BIT)
    {
        luaL_error(l, "size error: argument %d size (%d bits) exceeds the lua_Number size (%d bits)", 
                arg_index, elem->size, sizeof(lua_Integer) * CHAR_BIT);
    }

    if(elem->endianess != EE_DEFAULT)
    {
        luaL_error(l, "wrong format: unsupported endianess in argument %d", arg_index);
    }

    if(elem->size == sizeof(float) * CHAR_BIT)
    {
        float tmp = (float)value;
        basic_pack_bin(l, elem, (unsigned char *)&tmp, sizeof(tmp), state);
    }
    else if(elem->size == sizeof(double) * CHAR_BIT)
    {
        double tmp = value;
        basic_pack_bin(l, elem, (unsigned char *)&tmp, sizeof(tmp), state);
    }
    else
    {
        luaL_error(l, "size error: unsupported size %d for argument %d", 
                elem->size, arg_index);
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
        pack_int(l, elem, arg_index, state);
    }
    else if(elem->type == ET_BINARY)
    {
       pack_bin(l, elem, arg_index, state);
    }
    else if(elem->type == ET_FLOAT)
    {
       pack_float(l, elem, arg_index, state);
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
        size_t i;
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
        unsigned char result_buffer[255];
        memcpy(result_buffer, current_byte, bytes_to_copy);
        result = toint(l, elem, arg_index, result_buffer, bytes_to_copy); 
    }
    else
    {
        /* check for source end */
        size_t end_bit = state->current_bit + elem->size;
        const unsigned char *end_byte = state->source + end_bit / CHAR_BIT;

        size_t bytes_to_copy = bits_to_bytes(elem->size);
        unsigned char result_buffer[255];
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

        result = toint(l, elem, arg_index, result_buffer, bytes_to_copy); 
        result = clear_unused_bits(result, elem->size);
    }
    state->current_bit += elem->size;
    state->source_bits -= elem->size;
    return result;
}

/*
 * name
 *      grow_unpack_stack
 *
 * description
 *      grow the stack to have space for return values
 *      when unpacking using simple const increment
 *
 * paramenters
 *      l - lua state
 *      state - unpack state passed between invocations
 */
static void grow_unpack_stack(lua_State *l, UNPACK_STATE *state)
{
    if(state->return_count % 32 == 1)
    {
        int result = lua_checkstack(l, 32);
        if(!result)
        {
            luaL_error(l, "too many elements to unpack (%d)", state->return_count);
        }
    }
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
    grow_unpack_stack(l, state);
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
        unsigned char *result = (unsigned char *)luaL_prepbuffer(&b);
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
    grow_unpack_stack(l, state);
    luaL_pushresult(&b);
}

/*
 * name
 *      unpack_float
 *
 * description
 *      unpack floating point number from input buffer and push it onto lua stack. 
 *      update the number of return values
 *
 * paramenters
 *      l - lua state
 *      elem - element description
 *      arg_index - number of element in format string. starts from 1 
 *      state - unpack state passed between invocations
 *
 * throws
 *      size error - requested length is greater then remaining part of input
 *      size error - unsupported float size
 *
 * future work
 *      add support for different architectures
 */
static void unpack_float(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, UNPACK_STATE *state)
{
    if(elem->size > state->source_bits)
    {
        luaL_error(l, "size error: requested length for element %d is greater then remaining part of input", arg_index);
    }

	size_t float_size = elem->size / CHAR_BIT;
    unsigned char buff[255];
    size_t i;
    for(i = 0; i < float_size; ++i)
    {
        ELEMENT_DESCRIPTION tmp_elem;
        tmp_elem.size = CHAR_BIT;
        tmp_elem.endianess = EE_BIG;
        tmp_elem.type = ET_INTEGER; 
        buff[i] = unpack_int_no_push(l, &tmp_elem, arg_index, state);
    }

    grow_unpack_stack(l, state);
    if(elem->size == sizeof(float) * CHAR_BIT)
    {
        lua_pushnumber(l, *(float *)buff);
    }
    else if(elem->size == sizeof(double) * CHAR_BIT)
    {
        lua_pushnumber(l, *(double *)buff);
    }
    else
    {
        luaL_error(l, "size error: unsupported float size %d", elem->size);
    }
    ++state->return_count;
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
    else if(elem->type == ET_FLOAT)
    {
        unpack_float(l, elem, arg_index, state);
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
static int compare_token(const char *keyword, const char *token, size_t len)
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
            return (ELEMENT_TYPE)i;
        } 
        ++i;
    }
    return (ELEMENT_TYPE)luaL_error(l, "wrong format: unexpected type token (%s)", token); 
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
            return (ELEMENT_ENDIANESS)i;
        } 
        ++i;
    }
    return (ELEMENT_ENDIANESS)luaL_error(l, "wrong format: unexpected endianess token (%s)", token); 
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
 *      parse_format
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
 */
static void parse_format(lua_State *l, ELEM_HANDLER handler, void *arg)
{
    size_t len = 0;
    const char *format = luaL_checklstring(l, 1, &len);

    const char *token = format;
    size_t token_len = 0;
    ELEMENT_DESCRIPTION elem;
    memset(&elem, 0, sizeof(elem));
    /* allow leading space */
    PARSE_STATE state = SPACE_STATE;
    int argnum = 2;
    size_t i = 0;
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
 *      parse_bitmatch
 *
 * description
 *      iterate over array of elements and call handler for each
 *
 * paramenters
 *      l - lua state
 *      handler - handler for the element (un/pack_elem)
 *      arg - opaque argument passed between handlers
 */
static void parse_bitmatch(lua_State *l, ELEM_HANDLER handler, void *arg)
{
    BITMATCH *bitmatch = get_bitmatch(l, 1);
    size_t i;
    for(i = 0; i < bitmatch->element_count; ++i)
    {
        handler(l, &bitmatch->elements[i], i + 2, arg);
    }
}

/*
 * name
 *      parse
 *
 * description
 *      dispatch for actual parsing by input type
 *
 * paramenters
 *      l - lua state
 *      handler - element handler
 *      arg - agument passed between invocations
 *
 * throws
 *      argcheck error - when input is not string or bitmatch
 */
static void parse(lua_State *l, ELEM_HANDLER handler, void *arg)
{
    if(lua_isstring(l, 1))
    {
        parse_format(l, handler, arg);
    }
    else if(lua_isuserdata(l, 1) && get_bitmatch(l, 1) != NULL)
    {
        parse_bitmatch(l, handler, arg);
    }
    else
    {
        char message[255];
#ifdef WIN32
        _snprintf_s(
#else
		snprintf(
#endif
			message, sizeof(message), 
                "bitstring.bitmatch or string expected, got %s",
                lua_typename(l, lua_type(l, 1)));
        message[sizeof(message) - 1] = '\0';

        luaL_argcheck (l, 0, 1, message);
    }
}

/*
 * name
 *      get_substring
 *
 * description
 *      get substring based on parameters that specify substring in Lua style 
 *      as C style pointer/length substring
 *
 * paramenters
 *      l - lua state
 *      len - out parameter for requested length
 *      string_param - location of string parameter on stack
 *      start_param - location of start parameter on stack
 *      end_param - location of end parameter on stack
 *
 * returns
 *      pointer to relevant part of input string
 *      
 * throws
 *      invalid parameter - when start or stop are out of bounds
 *
 * rationale
 *      using string.sub in user code whould have the same effect 
 *      but it would be slightly less efficient and without
 *      bounds checking
 */
static const unsigned char *get_substring(
        lua_State *l, 
        size_t *len,
        int string_param,
        int start_param,
        int end_param)
{
    size_t original_length = 0;
    const unsigned char *original_start = (const unsigned char *)luaL_checklstring(l, string_param, &original_length); 

    /* Lua style */
    int start_position = 1;
    int end_position = original_length;

    /* C style */
    size_t start_offset = 0;
    size_t end_offset = original_length;

    if(lua_gettop(l) >= start_param)
    {
        start_position = luaL_checkinteger(l, start_param);
        if(start_position < 0)
        {
            start_offset = original_length + start_position;
        }
        else
        {
            start_offset = start_position - 1;
        }
    }
    if(lua_gettop(l) >= end_param)
    {
        end_position = luaL_checkinteger(l, end_param);
        if(end_position < 0)
        {
            end_offset = original_length + end_position + 1;
        }
        else
        {
            end_offset = end_position;
        }
    }

    if(start_offset >= end_offset)
    {
        luaL_error(l, "invalid parameter: start position %d, end position %d", 
                start_position, end_position);
    }

    if(end_offset > original_length)
    {
        luaL_error(l, "invalid parameter: start position %d, end position %d", 
                start_position, end_position);
    }
    *len = end_offset - start_offset;
    return original_start + start_offset;
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
    state.prep_buffer = (unsigned char *)luaL_prepbuffer(&b);
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
    const unsigned char *source = get_substring(l, &source_len, 2, 3, 4);

    UNPACK_STATE state;
    state.return_count = 0;
    state.current_bit = 0;
    state.source_bits = source_len * CHAR_BIT;
    state.source = source;
    state.source_end = source + source_len;
    parse(l, unpack_elem, (void *)&state);
    return state.return_count;
}

/*
 * name
 *      get_bitmatch
 *
 * description
 *      get a userdata from index and verify that it is bitstring.bitmatch
 *
 * paramenters
 *      l - lua state
 *      index - parameter index
 *
 * returns
 *      pointer to BITMATCH or NULL
 */
static BITMATCH *get_bitmatch(lua_State *l, int index)
{
    BITMATCH *bitmatch = (BITMATCH *)luaL_checkudata(l, index, "bitstring.bitmatch");
    return bitmatch;
}

/*
 * name
 *      realloc_bitmatch
 *
 * description
 *      allocate a new buffer for bitmatch and copy to it
 *      contents of previous bitmatch if not NULL
 *
 * paramenters
 *      l - lua state
 *      state - compile state
 *
 * returns
 *      pointer to new BITMATCH
 */
static BITMATCH *realloc_bitmatch(
        lua_State *l, 
        COMPILE_STATE *state)
{
    BITMATCH *current_bitmatch = state->bitmatch;
    size_t current_element_count = state->element_count;
    /* if reallocating existing bitmatch double number of elements */
    size_t new_element_count = current_bitmatch ? 
        current_element_count * 2 : current_element_count;
    size_t udata_size = sizeof(BITMATCH) + sizeof(ELEMENT_DESCRIPTION) * (new_element_count - 1);
    BITMATCH *new_bitmatch = (BITMATCH *)lua_newuserdata(l, udata_size);
    luaL_getmetatable(l, "bitstring.bitmatch");
    lua_setmetatable(l, -2);

    if(current_bitmatch != NULL)
    {
        size_t current_udata_size = 
            sizeof(BITMATCH) + sizeof(ELEMENT_DESCRIPTION) * (current_element_count - 1);
        memcpy(new_bitmatch, current_bitmatch, current_udata_size);
        /* remove the previous bitmatch from stack */
        lua_remove(l, -2);
    }
    state->bitmatch = new_bitmatch;
    state->bitmatch->element_count = 0;
    state->element_count = new_element_count;
    return new_bitmatch;
}

/*
 * name
 *      compile_elem
 *
 * description
 *      handler callback that is called by parse function
 *
 * paramenters
 *      l - lua state
 *      elem - element description
 *      arg_index - number of element in format string. starts from 1 
 *      arg - compile  state and intermediate results that are passed between
 *              invocations
 */
static void compile_elem(lua_State *l, ELEMENT_DESCRIPTION *elem, int arg_index, void *arg)
{
    COMPILE_STATE *state = (COMPILE_STATE *)arg;
    if(state->current == state->element_count)
    {
        realloc_bitmatch(l, state);
    }
    memcpy(&state->bitmatch->elements[state->current], elem, sizeof(ELEMENT_DESCRIPTION)); 
    ++state->current;
}

/*
 * name
 *      l_compile
 *
 * description
 *      lua_CFunction for compiling format string to bitmatch array
 *
 * paramenters
 *      l - lua state
 *
 * returns
 *      1
 */
static int l_compile(lua_State *l)
{
    size_t default_element_count = 32;
    COMPILE_STATE state;
    state.current = 0;
    state.element_count = default_element_count;
    state.bitmatch = NULL;
    realloc_bitmatch(l, &state);
    parse(l, compile_elem, (void *)&state);
    state.bitmatch->element_count = state.current;
    return 1;
}

#include "bitstring/lhexdump.c"
#include "bitstring/lbindump.c"

static const struct luaL_reg bitstring [] = 
{
    {"pack", l_pack},
    {"unpack", l_unpack},
    {"compile", l_compile},
    {"hexdump", l_hexdump},
    {"hexstream", l_hexstream},
    {"fromhexstream", l_fromhexstream},
    {"bindump", l_bindump},
    {"binstream", l_binstream},
    {"frombinstream", l_frombinstream},
    {NULL, NULL}  /* sentinel */
};

static int bitmatch_gc(lua_State *l)
{
    BITMATCH *bitmatch = (BITMATCH *)lua_touserdata(l, 1);
    /*
    printf("deleting bitmatch with %d elements\n", bitmatch->element_count);
    */
    return 0;
}

static void init_bitmatch_type(lua_State *l)
{
    luaL_newmetatable(l, "bitstring.bitmatch");
    lua_pushstring(l, "__gc");
    lua_pushcfunction(l, bitmatch_gc);
    lua_settable(l, -3);
}

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
#ifdef WIN32
extern "C" __declspec(dllexport) int luaopen_bitstring(lua_State *l) 
#else
int luaopen_bitstring(lua_State *l) 
#endif
{
    init_bitmatch_type(l);
    luaL_openlib(l, "bitstring", bitstring, 0);
    return 1;
}


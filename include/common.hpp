// This is free and unencumbered software released into the public domain.

// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.

// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

// For more information, please refer to <http://unlicense.org/>

#ifndef __COMMON_HPP_INCLUDED__
#define __COMMON_HPP_INCLUDED__

#include <assert.h>
#include <deque>
#include <list>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <uv.h>

#define CQL_LOG_CRITICAL 0x00
#define CQL_LOG_ERROR    0x01
#define CQL_LOG_INFO     0x02
#define CQL_LOG_DEBUG    0x03

#define CQL_ERROR_NO_ERROR          0
#define CQL_ERROR_SSL_CERT          1000000
#define CQL_ERROR_SSL_PRIVATE_KEY   1000001
#define CQL_ERROR_SSL_CA_CERT       1000002
#define CQL_ERROR_SSL_CRL           1000003
#define CQL_ERROR_SSL_READ          1000004
#define CQL_ERROR_SSL_WRITE         1000005
#define CQL_ERROR_SSL_READ_WAITING  1000006
#define CQL_ERROR_SSL_WRITE_WAITING 1000007
#define CQL_ERROR_NO_STREAMS        1000008

#define CQL_OPCODE_ERROR        0x00
#define CQL_OPCODE_STARTUP      0x01
#define CQL_OPCODE_READY        0x02
#define CQL_OPCODE_AUTHENTICATE 0x03
#define CQL_OPCODE_CREDENTIALS  0x04
#define CQL_OPCODE_OPTIONS      0x05
#define CQL_OPCODE_SUPPORTED    0x06
#define CQL_OPCODE_QUERY        0x07
#define CQL_OPCODE_RESULT       0x08
#define CQL_OPCODE_PREPARE      0x09
#define CQL_OPCODE_EXECUTE      0x0A
#define CQL_OPCODE_REGISTER     0x0B
#define CQL_OPCODE_EVENT        0x0C

#define CQL_CONSISTENCY_ANY          0x0000
#define CQL_CONSISTENCY_ONE          0x0001
#define CQL_CONSISTENCY_TWO          0x0002
#define CQL_CONSISTENCY_THREE        0x0003
#define CQL_CONSISTENCY_QUORUM       0x0004
#define CQL_CONSISTENCY_ALL          0x0005
#define CQL_CONSISTENCY_LOCAL_QUORUM 0x0006
#define CQL_CONSISTENCY_EACH_QUORUM  0x0007
#define CQL_CONSISTENCY_SERIAL       0x0008
#define CQL_CONSISTENCY_LOCAL_SERIAL 0x0009
#define CQL_CONSISTENCY_LOCAL_ONE    0x000A

uv_buf_t
alloc_buffer(
    size_t suggested_size)
{
    return uv_buf_init(new char[suggested_size], suggested_size);
}

uv_buf_t
alloc_buffer(
    uv_handle_t *handle,
    size_t       suggested_size)
{
    (void) handle;
    return alloc_buffer(suggested_size);
}

void
free_buffer(
    uv_buf_t buf)
{
    delete buf.base;
}

void
clear_buffer_deque(
    std::deque<uv_buf_t>& buffers)
{
    for (std::deque<uv_buf_t>::iterator it = buffers.begin();
         it != buffers.end();
         ++it)
    {
        free_buffer(*it);
    }
    buffers.clear();
}

#endif

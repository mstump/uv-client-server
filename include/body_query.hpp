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

#ifndef __QUERY_HPP_INCLUDED__
#define __QUERY_HPP_INCLUDED__

#include "body.hpp"

#define CQL_QUERY_FLAG_VALUES             0x01
#define CQL_QUERY_FLAG_SKIP_METADATA      0x02
#define CQL_QUERY_FLAG_PAGE_SIZE          0x04
#define CQL_QUERY_FLAG_PAGING_STATE       0x08
#define CQL_QUERY_FLAG_SERIAL_CONSISTENCY 0x10

class body_query_t
    : public body_t
{
private:
    typedef std::pair<char*, size_t> value_t;
    typedef std::list<value_t>       value_collection_t;

    std::string        _query;
    int16_t            _consistency;
    size_t             _page_size;
    bool               _page_size_set;
    char*              _paging_state;
    size_t             _paging_state_size;
    bool               _serial_consistent;
    int16_t            _serial_consistency;
    value_collection_t _values;

public:
    body_query_t() :
        _consistency(CQL_CONSISTENCY_ANY),
        _page_size(0),
        _page_size_set(false),
        _paging_state(NULL),
        _paging_state_size(0),
        _serial_consistent(false),
        _serial_consistency(CQL_CONSISTENCY_SERIAL)
    {}

    ~body_query_t()
    {
        delete _paging_state;
    }

    uint8_t
    opcode()
    {
        return CQL_OPCODE_QUERY;
    }

    void
    page_size(
        size_t size)
    {
        _page_size_set = true;
        _page_size     = size;
    }

    void
    page_state(
        const char* state,
        size_t      size)
    {
        memcpy(_paging_state, state, size);
        _paging_state_size = size;
    }

    void
    add_value(
        char*  value,
        size_t size)
    {
        _values.push_back(std::make_pair(value, size));
    }

    void
    consistency(
        int16_t consistency)
    {
        _consistency = consistency;
    }

    void
    serial_consistency(
        int16_t consistency)
    {
        _serial_consistent = true;
        _serial_consistency = consistency;
    }

    bool
    consume(
        char*  buffer,
        size_t size)
    {
        (void) buffer;
        (void) size;
        return true;
    }

    bool
    prepare(
        size_t  reserved,
        char**  output,
        size_t& size)
    {
        uint8_t flags = 0x00;
        size          = reserved + sizeof(int32_t) + _query.size() + sizeof(int16_t) + 1;

        if (_serial_consistent) {
            flags |= CQL_QUERY_FLAG_SERIAL_CONSISTENCY;
            size  += sizeof(int16_t);
        }

        if (_paging_state) {
            flags |= CQL_QUERY_FLAG_PAGING_STATE;
            size += _paging_state_size;
        }

        if (!_values.empty()) {
            for (value_collection_t::const_iterator it = _values.begin();
                 it != _values.end();
                 ++it)
            {
                size += it->second;
            }
            flags |= CQL_QUERY_FLAG_VALUES;
        }

        if (_page_size_set) {
            size += sizeof(int32_t);
            flags |= CQL_QUERY_FLAG_PAGE_SIZE;
        }

        if (_serial_consistent) {
            size += sizeof(int16_t);
            flags |= CQL_QUERY_FLAG_SERIAL_CONSISTENCY;
        }

        *output = new char[size];
        char* buffer = encode_long_string(*output, _query.c_str(), _query.size());
        buffer = encode_short(buffer, _consistency);
        buffer = encode_byte(buffer, flags);

        if (!_values.empty()) {
            buffer = encode_short(buffer, _values.size());
            for (value_collection_t::const_iterator it = _values.begin();
                 it != _values.end();
                 ++it)
            {
                buffer = encode_string(buffer, it->first, it->second);
            }
        }

        if (_page_size_set) {
            buffer = encode_int(buffer, _page_size);
        }

        if (_paging_state) {
            buffer = encode_string(buffer, _paging_state, _paging_state_size);
        }

        if (_serial_consistent) {
            buffer = encode_short(buffer, _serial_consistency);
        }

        return true;
    }

private:
    body_query_t(const body_query_t&) {}
    void operator=(const body_query_t&) {}
};

#endif
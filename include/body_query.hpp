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

#include <list>
#include <string>
#include <utility>
#include <vector>
#include "body.hpp"

#define CQL_QUERY_FLAG_VALUES             0x01
#define CQL_QUERY_FLAG_SKIP_METADATA      0x02
#define CQL_QUERY_FLAG_PAGE_SIZE          0x04
#define CQL_QUERY_FLAG_PAGING_STATE       0x08
#define CQL_QUERY_FLAG_SERIAL_CONSISTENCY 0x10

class BodyQuery
    : public Body {
 private:
  typedef std::pair<char*, size_t> value_t;
  typedef std::list<value_t>       value_collection_t;

  std::string        _query;
  int16_t            _consistency;
  size_t             _page_size;
  bool               _page_size_set;
  std::vector<char>  _paging_state;
  bool               _serial_consistent;
  int16_t            _serial_consistency;
  value_collection_t _values;

 public:
  BodyQuery() :
      _consistency(CQL_CONSISTENCY_ANY),
      _page_size(0),
      _page_size_set(false),
      _serial_consistent(false),
      _serial_consistency(CQL_CONSISTENCY_SERIAL)
  {}

  uint8_t
  opcode() {
    return CQL_OPCODE_QUERY;
  }

  void
  query_string(
      char*  input,
      size_t size) {
    _query.assign(input, size);
  }

  void
  query_string(
      const std::string& input) {
    _query = input;
  }

  void
  page_size(
      size_t size) {
    _page_size_set = true;
    _page_size     = size;
  }

  void
  paging_state(
      const char* state,
      size_t      size) {
    _paging_state.resize(size);
    _paging_state.assign(state, state + size);
  }

  void
  add_value(
      char*  value,
      size_t size) {
    _values.push_back(std::make_pair(value, size));
  }

  void
  consistency(
      int16_t consistency) {
    _consistency = consistency;
  }

  void
  serial_consistency(
      int16_t consistency) {
    _serial_consistent = true;
    _serial_consistency = consistency;
  }

  bool
  consume(
      char*  buffer,
      size_t size) {
    (void) buffer;
    (void) size;
    return true;
  }

  bool
  prepare(
      size_t  reserved,
      char**  output,
      size_t& size) {
    uint8_t  flags  = 0x00;
    size            = reserved + sizeof(int32_t);
    size           += _query.size() + sizeof(int16_t) + 1;

    if (_serial_consistent) {
      flags |= CQL_QUERY_FLAG_SERIAL_CONSISTENCY;
      size  += sizeof(int16_t);
    }

    if (!_paging_state.empty()) {
      flags |= CQL_QUERY_FLAG_PAGING_STATE;
      size += (sizeof(int16_t) + _paging_state.size());
    }

    if (!_values.empty()) {
      for (value_collection_t::const_iterator it = _values.begin();
           it != _values.end();
           ++it) {
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

    char* buffer = encode_long_string(
        *output + reserved,
        _query.c_str(),
        _query.size());

    buffer = encode_short(buffer, _consistency);
    buffer = encode_byte(buffer, flags);

    if (!_values.empty()) {
      buffer = encode_short(buffer, _values.size());
      for (value_collection_t::const_iterator it = _values.begin();
           it != _values.end();
           ++it) {
        buffer = encode_string(buffer, it->first, it->second);
      }
    }

    if (_page_size_set) {
      buffer = encode_int(buffer, _page_size);
    }

    if (!_paging_state.empty()) {
      buffer = encode_string(buffer, &_paging_state[0], _paging_state.size());
    }

    if (_serial_consistent) {
      buffer = encode_short(buffer, _serial_consistency);
    }

    return true;
  }

 private:
  BodyQuery(const BodyQuery&) {}
  void operator=(const BodyQuery&) {}
};

#endif

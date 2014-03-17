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

#ifndef __BODY_SUPPORTED_HPP_INCLUDED__
#define __BODY_SUPPORTED_HPP_INCLUDED__

#include <list>
#include <string>

#include "body.hpp"

struct BodySupported
    : public Body {
  std::list<std::string> compression;
  std::list<std::string> cql_versions;

  BodySupported()
  {}

  uint8_t
  opcode() {
    return CQL_OPCODE_SUPPORTED;
  }

  bool
  consume(
      char*  buffer,
      size_t size) {
    (void) size;
    string_multimap_t supported;

    decode_string_multimap(buffer, supported);
    string_multimap_t::const_iterator it = supported.find("COMPRESSION");
    if (it != supported.end()) {
      compression = it->second;
    }

    it = supported.find("CQL_VERSION");
    if (it != supported.end()) {
      cql_versions = it->second;
    }
    return true;
  }

  bool
  prepare(
      size_t  reserved,
      char**  output,
      size_t& size) {
    (void) reserved;
    (void) output;
    (void) size;
    return false;
  }

 private:
  BodySupported(const BodySupported&) {}
  void operator=(const BodySupported&) {}
};

#endif

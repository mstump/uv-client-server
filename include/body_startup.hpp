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

#ifndef __BODY_STARTUP_HPP_INCLUDED__
#define __BODY_STARTUP_HPP_INCLUDED__

#include <map>
#include <string>

#include "body.hpp"

struct BodyStartup
    : public Body {
  std::unique_ptr<char> guard;
  std::string           cql_version;
  std::string           compression;

  BodyStartup() :
      cql_version("3.0.0"),
      compression("")
  {}

  uint8_t
  opcode() {
    return CQL_OPCODE_STARTUP;
  }

  bool
  consume(
      char*  buffer,
      size_t size) {
    (void) size;

    OptionsCollection options;
    decode_string_map(buffer, options);
    OptionsCollection::const_iterator it = options.find("COMPRESSION");
    if (it != options.end()) {
      compression = it->second;
    }

    it = options.find("CQL_VERSION");
    if (it != options.end()) {
      cql_version = it->second;
    }
    return true;
  }

  bool
  prepare(
      size_t  reserved,
      char**  output,
      size_t& size) {
    size = reserved + sizeof(int16_t);

    std::map<std::string, std::string> options;
    if (!compression.empty()) {
      const char* key = "COMPRESSION";
      size += (sizeof(int16_t) + strlen(key));
      size += (sizeof(int16_t) + compression.size());
      options[key] = compression;
    }

    if (!cql_version.empty()) {
      const char* key = "CQL_VERSION";
      size += (sizeof(int16_t) + strlen(key));
      size += (sizeof(int16_t) + cql_version.size());
      options[key] = cql_version;
    }

    *output = new char[size];
    encode_string_map(*output + reserved, options);
    return true;
  }

 private:
  typedef std::map<std::string, std::string> OptionsCollection;

  BodyStartup(const BodyStartup&) {}
  void operator=(const BodyStartup&) {}
};

#endif

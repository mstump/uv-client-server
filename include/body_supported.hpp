

#ifndef __BODY_SUPPORTED_HPP_INCLUDED__
#define __BODY_SUPPORTED_HPP_INCLUDED__

#include <list>
#include <string>

#include "body.hpp"

namespace cql {

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
}
#endif

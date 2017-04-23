/* Test cases for the status line handlers.
 *
 * HTTP prefaces server headers with a status line. There's a few functions to
 * look up and otherwise deal with those in the code, and this file tests them.
 *
 * See also:
 * * Project Documentation: https://ef.gy/documentation/cxxhttp
 * * Project Source Code: https://github.com/ef-gy/cxxhttp
 * * Licence Terms: https://github.com/ef-gy/cxxhttp/blob/master/COPYING
 *
 * @copyright
 * This file is part of the cxxhttp project, which is released as open source
 * under the terms of an MIT/X11-style licence, described in the COPYING file.
 */
#include <iostream>

#include <ef.gy/test-case.h>

#include <cxxhttp/http-status.h>

using namespace cxxhttp;

/* Test status description lookup.
 * @log Test output stream.
 *
 * Performs a few lookups, including deliberately invalid status codes, to make
 * sure that works as expected.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testDescription(std::ostream &log) {
  struct sampleData {
    unsigned in;
    std::string out;
  };

  std::vector<sampleData> tests{
      {0, "Other Status"}, {100, "Continue"}, {404, "Not Found"},
  };

  for (const auto &tt : tests) {
    const auto v = http::statusDescription(tt.in);
    if (v != tt.out) {
      log << "http::statusDescription(" << tt.in << ")='" << v
          << "', expected '" << tt.out << "'\n";
      return false;
    }
  }

  return true;
}

/* Test status parsing.
 * @log Test output stream.
 *
 * Parses some status lines to see if that works as expected.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testParse(std::ostream &log) {
  struct sampleData {
    std::string in;
    bool defined;
    unsigned code;
    std::string protocol, description;
  };

  std::vector<sampleData> tests{
      {"", false, 0, "", ""},
      {"frob", false, 0, "", ""},
      {"HTTP/1.1 glorb", false, 0, "", ""},
      {"HTTP/1.1 999 glorb\r", true, 999, "HTTP/1.1", "glorb"},
      {"HTTP/1.0 200 OK", true, 200, "HTTP/1.0", "OK"},
  };

  for (const auto &tt : tests) {
    const auto v = http::parse(tt.in);
    if (bool(v) != tt.defined) {
      log << "http::parse(" << tt.in << ") has incorrect result\n";
      return false;
    }
    if (bool(v)) {
      if (v->code != tt.code) {
        log << "http::parse(" << tt.in << ").code='" << v->code
            << "', expected'" << tt.code << "'\n";
        return false;
      }
      if (v->protocol != tt.protocol) {
        log << "http::parse(" << tt.in << ").protocol='" << v->protocol
            << "', expected'" << tt.protocol << "'\n";
        return false;
      }
      if (v->description != tt.description) {
        log << "http::parse(" << tt.in << ").description='" << v->description
            << "', expected'" << tt.description << "'\n";
        return false;
      }
    }
  }

  return true;
}

namespace test {
using efgy::test::function;

static function description(testDescription);
static function parse(testParse);
}

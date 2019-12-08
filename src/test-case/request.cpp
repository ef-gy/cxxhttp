/* Test cases for the request line handlers.
 *
 * HTTP prefaces client headers with a request line, and the parsing of which is
 * tested here.
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

#include <cxxhttp/http-request.h>
#include <ef.gy/test-case.h>

using namespace cxxhttp;

/* Test status parsing.
 * @log Test output stream.
 *
 * Parses some request lines to see if that works as expected. The turns them
 * back into strings to test that part as well.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testParse(std::ostream &log) {
  struct sampleData {
    std::string in;
    bool valid;
    std::string method, resource, protocol, out;
  };

  std::vector<sampleData> tests{
      {"", false, "", "", "HTTP/0.0", "FAIL * HTTP/0.0\r\n"},
      {"GET /foo HTTP/1.2", true, "GET", "/foo", "HTTP/1.2",
       "GET /foo HTTP/1.2\r\n"},
      {"OPTIONS * HTTP/1.1", true, "OPTIONS", "*", "HTTP/1.1",
       "OPTIONS * HTTP/1.1\r\n"},
      {"GET /?a=b HTTP/1.1", true, "GET", "/?a=b", "HTTP/1.1",
       "GET /?a=b HTTP/1.1\r\n"},
      {"GET /?a=b&c=d HTTP/1.1", true, "GET", "/?a=b&c=d", "HTTP/1.1",
       "GET /?a=b&c=d HTTP/1.1\r\n"},
  };

  for (const auto &tt : tests) {
    const http::requestLine v(tt.in);
    if (v.valid() != tt.valid) {
      log << "http::requestLine(" << tt.in << ") has incorrect validity\n";
      return false;
    }
    if (v.protocol() != tt.protocol) {
      log << "http::requestLine(" << tt.in << ").protocol='" << v.protocol()
          << "', expected'" << tt.protocol << "'\n";
      return false;
    }
    if (v.method != tt.method) {
      log << "http::requestLine(" << tt.in << ").method='" << v.method
          << "', expected'" << tt.method << "'\n";
      return false;
    }
    if (std::string(v.resource) != tt.resource) {
      log << "http::requestLine(" << tt.in << ").resource='"
          << std::string(v.resource) << "', expected'" << tt.resource << "'\n";
      return false;
    }
    if (v.assemble() != tt.out) {
      log << "http::requestLine(" << tt.in << ").assemble='" << v.assemble()
          << "', expected'" << tt.out << "'\n";
      return false;
    }
  }

  return true;
}

namespace test {
using efgy::test::function;

static function parse(testParse);
}  // namespace test

/* Test cases for the error handling helper code.
 *
 * These tests exercise `http::error`, which is used to send hopefully-useful
 * error messages back to HTTP clients when something goes wrong somewhere.
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

#include <ef.gy/test-case.h>

#define ASIO_DISABLE_THREADS
#define NO_DEFAULT_OPTIONS
#include <cxxhttp/http-error.h>
#include <cxxhttp/http-test.h>

using namespace cxxhttp;

/* Test the error handler.
 * @log Test output stream.
 *
 * Does a full end-to-end exercise of the error handler, with very controlled
 * inputs.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testErrorHandler(std::ostream &log) {
  struct sampleData {
    std::string request;
    std::set<std::string> allow;
    unsigned status;
    http::headers header;
    std::string message;
  };

  std::vector<sampleData> tests{
      {"FOO / HTTP/1.1",
       {},
       400,
       {{"Content-Type", "text/markdown"}},
       "# Client Error\n\nAn error occurred while processing your request. "
       "That's all I know.\n"},
      {"FOO / HTTP/1.1",
       {"GET", "BLARGH"},
       405,
       {{"Allow", "BLARGH,GET"}, {"Content-Type", "text/markdown"}},
       "# Method Not Allowed\n\nAn error occurred while processing your "
       "request. That's all I know.\n"},
  };

  for (const auto &tt : tests) {
    http::recorder sess;
    std::smatch matches;

    sess.inboundRequest = tt.request;

    http::error<http::recorder> e(sess);

    e.allow = tt.allow;
    e.reply(tt.status);

    if (sess.status != tt.status) {
      log << "error() produced an unexpected status code: " << sess.status
          << ", expected " << tt.status << "\n";
      return false;
    }
    if (sess.header != tt.header) {
      log << "error() produced unexpected headers.\n";
      return false;
    }
    if (sess.message != tt.message) {
      log << "error() produced an unexpected message: '" << sess.message
          << "' expected: '" << tt.message << "'\n";
      return false;
    }
  }

  return true;
}

namespace test {
using efgy::test::function;

static function errorHandler(testErrorHandler);
}

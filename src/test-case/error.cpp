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
    std::string request, accept;
    std::set<std::string> allow;
    unsigned status;
    std::string message;
  };

  std::vector<sampleData> tests{
      {"FOO / HTTP/1.1",
       "text/*",
       {},
       400,
       "HTTP/1.1 400 Client Error\r\n"
       "Connection: close\r\n"
       "Content-Length: 84\r\n"
       "Content-Type: text/markdown\r\n"
       "\r\n"
       "# Client Error\n\nAn error occurred while processing your request. "
       "That's all I know.\n"},
      {"FOO / HTTP/1.1",
       "",
       {"GET", "BLARGH"},
       405,
       "HTTP/1.1 405 Method Not Allowed\r\n"
       "Allow: BLARGH,GET\r\n"
       "Connection: close\r\n"
       "Content-Length: 90\r\n"
       "Content-Type: text/markdown\r\n"
       "\r\n"
       "# Method Not Allowed\n\nAn error occurred while processing your "
       "request. That's all I know.\n"},
      {"FOO / HTTP/1.1",
       "application/frob",
       {"GET", "BLARGH"},
       405,
       "HTTP/1.1 405 Method Not Allowed\r\n"
       "Allow: BLARGH,GET\r\n"
       "Connection: close\r\n"
       "Content-Length: 157\r\n"
       "Content-Type: text/markdown\r\n"
       "\r\n"
       "# Method Not Allowed\n\nAn error occurred while processing your "
       "request. Additionally, content type negotiation for this error page "
       "failed. That's all I know.\n"},
  };

  for (const auto &tt : tests) {
    http::sessionData sess;
    std::smatch matches;

    sess.inboundRequest = tt.request;
    if (!tt.accept.empty()) {
      sess.inbound.header["Accept"] = tt.accept;
    }

    http::error e(sess);

    e.allow = tt.allow;
    e.reply(tt.status);

    if (sess.outboundQueue.size() == 0) {
      log << "nothing was sent.\n";
      return false;
    }

    const auto &m = sess.outboundQueue.front();
    if (m != tt.message) {
      log << "error() produced an unexpected message: '" << m << "' expected: '"
          << tt.message << "'\n";
      return false;
    }
  }

  return true;
}

namespace test {
using efgy::test::function;

static function errorHandler(testErrorHandler);
}  // namespace test

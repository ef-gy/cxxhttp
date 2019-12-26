/* Test cases for the TRACE implementation.
 *
 * TRACE is a debugging tool built into HTTP that replies to any request that is
 * sent with the request and header lines that were sent.
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
#include <cxxhttp/httpd-trace.h>

using namespace cxxhttp;

/* Test the trace handler.
 * @log Test output stream.
 *
 * Does a full end-to-end exercise of the trace handler, with very controlled
 * inputs.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testTraceHandler(std::ostream &log) {
  struct sampleData {
    std::string request;
    std::set<std::string> headers;
    std::string message;
  };

  std::vector<sampleData> tests{
      {"TRACE / HTTP/1.1",
       {"Host: none", "Foo: Bar"},
       "HTTP/1.1 200 OK\r\n"
       "Content-Length: 40\r\n"
       "Content-Type: message/http\r\n"
       "\r\n"
       "TRACE / HTTP/1.1\r\nFoo: Bar\r\nHost: none\r\n"},
  };

  for (const auto &tt : tests) {
    http::sessionData sess;
    std::smatch matches;

    sess.inboundRequest = tt.request;
    for (const auto &h : tt.headers) {
      sess.inbound.absorb(h);
    }

    httpd::trace::trace(sess, matches);

    if (sess.outboundQueue.size() == 0) {
      log << "nothing was sent.\n";
      return false;
    }

    const auto &m = sess.outboundQueue.front();
    if (m != tt.message) {
      log << "trace() produced an unexpected message: '" << m << "' expected: '"
          << tt.message << "'\n";
      return false;
    }
  }

  return true;
}

namespace test {
using efgy::test::function;

static function traceHandler(testTraceHandler);
}  // namespace test

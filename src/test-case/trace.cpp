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
#define NO_DEFAULT_TRACE
#include <cxxhttp/httpd-trace.h>

namespace cxxhttp {
namespace transport {
class fake {
 public:
  using endpoint = int;
  using acceptor = int;
};
}

namespace http {
template <>
class session<transport::fake, processor::server<transport::fake>>
    : public sessionData {
 public:
  unsigned status;
  headers header;
  std::string message;

  void reply(int pStatus, const headers &pHeader, const std::string &pMessage) {
    status = pStatus;
    header = pHeader;
    message = pMessage;
  }
};

using recorder = session<transport::fake, processor::server<transport::fake>>;
}
}

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
    unsigned status;
    http::headers header;
    std::string message;
  };

  std::vector<sampleData> tests{
      {"TRACE / HTTP/1.1",
       {"Host: none", "Foo: Bar"},
       200,
       {{"Content-Type", "message/http"}},
       "TRACE / HTTP/1.1\r\nFoo: Bar\r\nHost: none\r\n"},
  };

  for (const auto &tt : tests) {
    http::recorder sess;
    std::smatch matches;

    sess.inboundRequest = tt.request;
    for (const auto &h : tt.headers) {
      sess.inbound.absorb(h);
    }

    httpd::trace::trace<transport::fake>(sess, matches);

    if (sess.status != tt.status) {
      log << "trace() produced an unexpected status code: " << sess.status
          << ", expected " << tt.status << "\n";
      return false;
    }
    if (sess.header != tt.header) {
      log << "trace() produced an unexpected headers.\n";
      return false;
    }
    if (sess.message != tt.message) {
      log << "trace() produced an unexpected message: '" << sess.message
          << "' expected: '" << tt.message << "'\n";
      return false;
    }
  }

  return true;
}

namespace test {
using efgy::test::function;

static function traceHandler(testTraceHandler);
}

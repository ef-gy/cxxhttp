/* Test cases for the OPTIONS implementation.
 *
 * OPTIONS allows for querying allowed operations off a server, which helps
 * clients in determining what to do with a given server.
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
#include <cxxhttp/http-test.h>
#include <cxxhttp/httpd-options.h>

using namespace cxxhttp;

/* Test the options handler.
 * @log Test output stream.
 *
 * Does a full end-to-end exercise of the options handler, with very controlled
 * inputs.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testTraceHandler(std::ostream &log) {
  struct sampleData {
    std::string request;
    unsigned status;
    http::headers header;
    std::string message;
  };

  std::vector<sampleData> tests{
      {"OPTIONS / HTTP/1.1",
       200,
       {{"Allow", "OPTIONS"}, {"Content-Type", "text/markdown; charset=UTF-8"}},
       "# Applicable Resource Processors\n\n"
       " * _OPTIONS_ `^\\*|/.*`\n   no description available\n"},
  };

  httpd::servlet<transport::fake> fakeHandler(
      httpd::options::resource, httpd::options::options<transport::fake>,
      httpd::options::method);

  for (const auto &tt : tests) {
    http::recorder sess;
    std::smatch matches;

    sess.inboundRequest = tt.request;
    sess.connection.processor.servlets.insert(&fakeHandler);

    std::string resource = sess.inboundRequest.resource.path();
    std::regex_match(resource, matches, fakeHandler.resource);
    httpd::options::options<transport::fake>(sess, matches);

    if (sess.status != tt.status) {
      log << "options() produced an unexpected status code: " << sess.status
          << ", expected " << tt.status << "\n";
      return false;
    }
    if (sess.header != tt.header) {
      log << "options() produced unexpected headers.\n";
      return false;
    }
    if (sess.message != tt.message) {
      log << "options() produced an unexpected message: '" << sess.message
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

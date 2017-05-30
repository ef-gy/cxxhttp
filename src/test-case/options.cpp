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
bool testOptionsHandler(std::ostream &log) {
  struct sampleData {
    std::string request;
    http::headers inbound;
    std::string message;
  };

  std::vector<sampleData> tests{
      {"OPTIONS / HTTP/1.1",
       {{"Accept", "text/markdown"}},
       "HTTP/1.1 200 OK\r\n"
       "Allow: OPTIONS\r\n"
       "Content-Length: 171\r\n"
       "\r\n"
       "# Applicable Resource Processors\n\n"
       "The following servlets are built into the application and match the "
       "given resource:\n\n"
       " * _OPTIONS_ `^\\*|/.*`\n   no description available\n\n"},
  };

  http::servlet fakeHandler(httpd::options::resource, httpd::options::options,
                            httpd::options::method,
                            httpd::options::negotiations);

  for (const auto &tt : tests) {
    http::sessionData sess;
    std::smatch matches;

    sess.inboundRequest = tt.request;
    sess.inbound = {tt.inbound};

    std::string resource = sess.inboundRequest.resource.path();
    std::regex_match(resource, matches, fakeHandler.resource);
    httpd::options::options(sess, matches);

    if (sess.outboundQueue.size() == 0) {
      log << "nothing was sent.\n";
      return false;
    }

    const auto &m = sess.outboundQueue.front();
    if (m != tt.message) {
      log << "options() produced an unexpected message: '" << m
          << "' expected: '" << tt.message << "'\n";
      return false;
    }
  }

  return true;
}

namespace test {
using efgy::test::function;

static function optionsHandler(testOptionsHandler);
}

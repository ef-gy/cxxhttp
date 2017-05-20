/* End-to-end tests.
 *
 * This file is for full-on end-to-end tests. Eventually, anyway. Right now it's
 * just to make sure the server portions compile, and to make sure the coverage
 * reports actually cover the network bits.
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

#include <cstdio>

#define ASIO_DISABLE_THREADS
#include <ef.gy/test-case.h>

#include <cxxhttp/http-client.h>
#include <cxxhttp/httpd-options.h>
#include <cxxhttp/httpd-trace.h>

using namespace cxxhttp;

/* Set up a UNIX test server and query it.
 * @log Test output stream.
 *
 * The test server will be at `/tmp/cxxhttp-test.socket`.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testUNIX(std::ostream &log) {
  const char *name = "/tmp/cxxhttp-test.socket";
  bool result = true;

  std::remove(name);

  efgy::cli::options opts({"http:unix:/tmp/cxxhttp-test.socket"});

  net::endpoint<transport::unix> lookup(name);
  efgy::beacons<http::client<transport::unix>> &clients =
      efgy::global<efgy::beacons<http::client<transport::unix>>>();
  cxxhttp::service &service = efgy::global<cxxhttp::service>();

  for (net::endpointType<transport::unix> endpoint : lookup) {
    http::client<transport::unix> *s =
        new http::client<transport::unix>(endpoint, clients, service);

    s->processor.query("GET", "/", {{"Host", name}, {"Keep-Alive", "none"}})
        .then([&result,
               &log](typename http::client<transport::unix>::session &session) {
          if (session.inboundStatus.code != 501) {
            log << "nothing implemented the GET method, but we got a "
                << session.inboundStatus.code << "\n";
            result = false;
          }
          if (session.content !=
              "# Not Implemented\n\n"
              "An error occurred while processing your request. "
              "That's all I know.\n") {
            log << "unexpected content:\n" << session.content << "\n";
            result = false;
          }

          efgy::global<cxxhttp::service>().stop();
        });
  }

  for (net::endpointType<transport::unix> endpoint : lookup) {
    http::client<transport::unix> *s =
        new http::client<transport::unix>(endpoint, clients, service);

    s->processor.query("GET ", "/", {{"Host", name}, {"Keep-Alive", "none"}})
        .then([&result,
               &log](typename http::client<transport::unix>::session &session) {
          if (session.inboundStatus.code != 400) {
            log << "we sent a bad request line, but got status code "
                << session.inboundStatus.code << "\n";
            result = false;
          }
          if (session.content !=
              "# Client Error\n\n"
              "An error occurred while processing your request. "
              "That's all I know.\n") {
            log << "unexpected content:\n" << session.content << "\n";
            result = false;
          }

          efgy::global<cxxhttp::service>().stop();
        });
  }

  efgy::global<cxxhttp::service>().run();

  return result;
}

namespace test {
using efgy::test::function;

static function UNIX(testUNIX);
}

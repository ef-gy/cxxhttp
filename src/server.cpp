/* "Hello World" HTTP server sample.
 *
 * An example HTTP server that serves a simple "Hello World!" on /, and a 404 on
 * all other resources.
 *
 * Call it like this:
 *
 *     $ ./server http:localhost:8080
 *
 * With localhost and 8080 being a host name and port of your choosing. Then,
 * while the programme is running, open a browser and go to
 * http://localhost:8080/ and you should see the familiar greeting.
 *
 * Contains a very basic HTTP client, primarily to test the library against an
 * HTTP server running on a UNIX socket.
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
#define ASIO_DISABLE_THREADS
#include <cxxhttp/httpd.h>

// Optional server features.
#include <cxxhttp/httpd-options.h>
#include <cxxhttp/httpd-quit.h>
#include <cxxhttp/httpd-trace.h>

#include <ef.gy/stream-json.h>

using namespace cxxhttp;

/* Hello World request handler
 * @session The HTTP session to answer on.
 *
 * This function serves the familiar "Hello World!" when called.
 *
 * @return 'true' (always, as we always reply).
 */
template <class transport>
static bool hello(typename net::http::server<transport>::session &session,
                  std::smatch &) {
  using efgy::json::json;
  using efgy::json::tag;

  const std::string message = "Hello World!";

  if (session.negotiated["Content-Type"] == "text/plain") {
    session.reply(200, message);
  } else if (session.negotiated["Content-Type"] == "application/json") {
    std::ostringstream s("");
    s << tag() << json(message);

    session.reply(200, s.str());
  }

  return true;
}

namespace tcp {
using cxxhttp::httpd::transport::tcp;
static httpd::servlet<tcp> hello("/", ::hello<tcp>, "GET",
                                 {{"Accept",
                                   "text/plain, application/json;q=0.9"}});
}

namespace unix {
using cxxhttp::httpd::transport::unix;
static httpd::servlet<unix> hello("/", ::hello<unix>, "GET",
                                  {{"Accept",
                                    "text/plain, application/json;q=0.9"}});
}

/* Main function for the HTTP demo
 * @argc Process argument count.
 * @argv Process argument vector
 *
 * Main function for the network server hello world programme.
 *
 * Sets up server(s) as per the given command line arguments. Invalid arguments
 * are ignored.
 *
 * @return 0 when nothing bad happened, 1 otherwise.
 */
int main(int argc, char *argv[]) { return io::main(argc, argv); }

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
#define USE_DEFAULT_IO_MAIN
#include <cxxhttp/httpd.h>

// Optional server features.
#include <cxxhttp/httpd-options.h>
#include <cxxhttp/httpd-trace.h>

#include <ef.gy/stream-json.h>

using namespace cxxhttp;

/* Hello World request handler
 * @session The HTTP session to answer on.
 *
 * This function serves the familiar "Hello World!" when called.
 */
template <class transport>
static void hello(typename http::server<transport>::session &session,
                  std::smatch &) {
  using efgy::json::json;
  using efgy::json::tag;

  if (session.inboundRequest.method == "POST") {
    session.reply(200, {{"Content-Type", session.header["Content-Type"]}},
                  session.content);
    return;
  }

  const std::string message = "Hello World!";

  if (session.outbound.header["Content-Type"] == "text/plain") {
    session.reply(200, message);
  } else if (session.outbound.header["Content-Type"] == "application/json") {
    std::ostringstream s("");
    s << tag() << json(message);

    session.reply(200, s.str());
  }
}

static const char *resource = "/";
static const char *method = "GET|POST";
static const http::headers negotiations{
    {"Accept", "text/plain, application/json;q=0.9"}};

namespace tcp {
static httpd::servlet<transport::tcp> hello(resource, ::hello<transport::tcp>,
                                            method, negotiations);
}

namespace unix {
static httpd::servlet<transport::unix> hello(resource, ::hello<transport::unix>,
                                             method, negotiations);
}

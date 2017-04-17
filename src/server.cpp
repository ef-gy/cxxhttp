/**\file
 * \ingroup example-programmes
 * \brief "Hello World" HTTP Server
 *
 * An example HTTP server that serves a simple "Hello World!" on /, and a 404 on
 * all other resources.
 *
 * Call it like this:
 * \code
 * $ ./server http:localhost:8080
 * \endcode
 *
 * With localhost and 8080 being a host name and port of your choosing. Then,
 * while the programme is running, open a browser and go to
 * http://localhost:8080/ and you should see the familiar greeting.
 *
 * \copyright
 * This file is part of the cxxhttp project, which is released as open source
 * under the terms of an MIT/X11-style licence, described in the COPYING file.
 *
 * \see Project Documentation: https://ef.gy/documentation/cxxhttp
 * \see Project Source Code: https://github.com/ef-gy/cxxhttp
 * \see Licence Terms: https://github.com/ef-gy/cxxhttp/blob/master/COPYING
 */

#define ASIO_DISABLE_THREADS
#include <cxxhttp/httpd.h>

// Optional server features.
#include <cxxhttp/httpd-options.h>
#include <cxxhttp/httpd-quit.h>
#include <cxxhttp/httpd-trace.h>

using namespace cxxhttp;

/**\brief Hello World request handler
 *
 * This function serves the familiar "Hello World!" when called.
 *
 * \param[out] session The HTTP session to answer on.
 *
 * \returns true (always, as we always reply).
 */
template <class transport>
static bool hello(typename net::http::server<transport>::session &session,
                  std::smatch &) {
  if (session.negotiated["Content-Type"] == "text/plain") {
    session.reply(200, "Hello World!");
  } else if (session.negotiated["Content-Type"] == "application/json") {
    session.reply(200, "\"Hello World!\"");
  }

  return true;
}

namespace tcp {
using asio::ip::tcp;
static httpd::servlet<tcp> hello("/", ::hello<tcp>, "GET",
                                 {{"Accept",
                                   "text/plain, application/json;q=0.9"}});
}

namespace unix {
using asio::local::stream_protocol;
static httpd::servlet<stream_protocol> hello(
    "/", ::hello<stream_protocol>, "GET",
    {{"Accept", "text/plain, application/json;q=0.9"}});
}

/**\brief Main function for the HTTP demo
 *
 * Main function for the network server hello world programme.
 *
 * Sets up server(s) as per the given command line arguments. Invalid arguments
 * are ignored.
 *
 * \param[in] argc Process argument count.
 * \param[in] argv Process argument vector
 *
 * \returns 0 when nothing bad happened, 1 otherwise.
 */
int main(int argc, char *argv[]) { return io::main(argc, argv); }

/**\file
 * \ingroup example-programmes
 * \brief "Hello World" HTTP/IRC Server
 *
 * An example HTTP server that serves a simple "Hello World!" on /, and a 404 on
 * all other resources. The programme also contains a sample IRC server that can
 * run in parallel, or instead of, the HTTP server.
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
 * You can also use 'irc' in place of 'http'. This would spawn an IRC server,
 * which you can connect to and use with your favourite IRC client.
 *
 * \copyright
 * This file is part of the libefgy project, which is released as open source
 * under the terms of an MIT/X11-style licence, described in the COPYING file.
 *
 * \see Project Documentation: https://ef.gy/documentation/libefgy
 * \see Project Source Code: https://github.com/ef-gy/libefgy
 * \see Licence Terms: https://github.com/ef-gy/libefgy/blob/master/COPYING
 */

#define ASIO_DISABLE_THREADS
#include <ef.gy/httpd.h>
#include <ef.gy/ircd.h>

using namespace efgy;

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
  session.reply(200, "Hello World!");

  return true;
}

namespace tcp {
using asio::ip::tcp;
static httpd::servlet<tcp> hello("/", ::hello<tcp>);
static httpd::servlet<tcp> quit("/quit", httpd::quit<tcp>);
}

namespace unix {
using asio::local::stream_protocol;
static httpd::servlet<stream_protocol> hello("/", ::hello<stream_protocol>);
static httpd::servlet<stream_protocol> quit("/quit",
                                            httpd::quit<stream_protocol>);
}

/**\brief Main function for the HTTP/IRC demo
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

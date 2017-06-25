/* asio.hpp HTTP server.
 *
 * This file implements an actual HTTP server with a simple servlet capability
 * in top of http.h.
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
#if !defined(CXXHTTP_HTTPD_H)
#define CXXHTTP_HTTPD_H

#include <cstdio>

#include <ef.gy/cli.h>

#include <cxxhttp/http-network.h>
#include <cxxhttp/http-stdio.h>

namespace cxxhttp {
namespace httpd {
namespace cli {
/* Set up an endpoint as an HTTP server.
 * @transport The transport type of the endpoint, e.g. transport::tcp.
 * @lookup The endpoint to bind to.
 * @servers The set of servers to add the newly set up instance to.
 * @service The I/O service to use, defaults to the global one.
 * @servlets The servlets to bind, defaults to the global set for the transport.
 *
 * This sets up an HTTP server on a new listening socket bound to whatever
 * `lookup` specifies. If that ends up being several different sockets, then
 * multiple servers are spawned.
 *
 * @return `true` if the setup was successful.
 */
template <class transport>
static bool setup(net::endpoint<transport> lookup,
                  efgy::beacons<http::server<transport>> &servers =
                      efgy::global<efgy::beacons<http::server<transport>>>(),
                  service & service = efgy::global<cxxhttp::service>(),
                  efgy::beacons<http::servlet> &
                      servlets = efgy::global<efgy::beacons<http::servlet>>()) {
  bool rv = false;

  for (net::endpointType<transport> endpoint : lookup) {
    auto &s = http::server<transport>::get(endpoint, servers, service);

    s.processor.servlets = servlets;

    rv = rv || true;
  }

  return rv;
}

/* Set up an HTTP server on STDIO.
 * @service The ASIO I/O service to initialise the server with; optional.
 * @servlets Which servlets to initialise the new server with; optional.
 *
 * This will initialise an HTTP server with all the normal trimmings on STDIO.
 * Primarily useful for debugging and for use with (x)inetd.
 *
 * @return `true` for success (always).
 */
static inline bool setup(service &service = efgy::global<cxxhttp::service>(),
                         efgy::beacons<http::servlet> &servlets =
                             efgy::global<efgy::beacons<http::servlet>>()) {
  static http::stdio::server server(service);
  server.processor.servlets = servlets;
  server.start();
  return true;
}

/* Set up an HTTP server on a TCP socket.
 * @match The matches from the TCP regex.
 *
 * This uses setup() to create a server on the interface specified with match[1]
 * and the port specified with match[2].
 * The server will have the default HTTP processor, with all registered TCP
 * servlets applied.
 *
 * @return 'true' if the setup was successful.
 */
static inline bool setupTCP(std::smatch &match) {
  return setup(net::endpoint<transport::tcp>(match[1], match[2]));
}

static efgy::cli::flag<bool> keepSocket(
    "keep-socket",
    "whether to keep a UNIX socket name if it already exists and fail");

/* Set up an HTTP server on a UNIX socket.
 * @match The matches from the UNIX regex.
 *
 * This uses setup() to create a server on the socket file specified with
 * match[1]. The server will have the default HTTP processor, with all
 * registered UNIX servlets applied.
 *
 * @return 'true' if the setup was successful.
 */
static inline bool setupUNIX(std::smatch &match) {
  const std::string socket = match[1];
  if (!keepSocket) {
    // ignore errors when unlinking the socket name; if it still exists later,
    // we'll get an error trying to listen on it, if it never existed then the
    // error is moot anyway.
    (void)std::remove(socket.c_str());
  }
  return setup(net::endpoint<transport::unix>(socket));
}

/* Set up an HTTP server on STDIO.
 * @match Matches from the CLI option regex; ignored.
 *
 * This will initialise an HTTP server with all the normal trimmings on STDIO.
 * Primarily useful for debugging and for use with (x)inetd.
 *
 * @return `true` for success (always).
 */
static inline bool setupSTDIO(std::smatch &match) { return setup(); }

/* TCP HTTP server CLI option.
 *
 * The format is `http:(interface-address):(port)`. The server that is set up
 * will have all available servlets registered.
 */
static efgy::cli::option TCP(
    "-{0,2}http:(.+):([0-9]+)", setupTCP,
    "listen for HTTP connections on the given host[1] and port[2]");

/* UNIX socket HTTP server CLI option.
 *
 * The format is `http:unix:(socket-file)`. The server that is set up will have
 * all available servlets registered.
 */
static efgy::cli::option UNIX(
    "-{0,2}http:unix:(.+)", setupUNIX,
    "listen for HTTP connections on the given unix socket[1]");

/* STDIO HTTP server CLI option.
 *
 * For when you want to talk on stdio. Because... testing things, maybe? Or
 * inetd. Yeah, let's go with inetd.
 */
static efgy::cli::option STDIO("-{0,2}http:stdio", setupSTDIO,
                               "process HTTP connections on STDIN and STDOUT");
}

namespace usage {
/* Create usage hint.
 *
 * This creates a brief textual summary of the available servlets, with the
 * regular expressions that are used to match them against incoming queries.
 *
 * @return A string with a summary of the available servlets.
 */
static std::string describe(void) {
  std::string rv;
  const auto &servlets = efgy::global<efgy::beacons<http::servlet>>();
  for (const auto &servlet : servlets) {
    rv += servlet->describe();
  }
  return rv;
}

/* Servlet hints.
 *
 * Enables a description of all available HTTP servlets for the `--help` flag.
 */
static efgy::cli::hint endpointHint("HTTP Endpoints", describe);
}
}
}

#endif

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

#include <ef.gy/cli.h>

#include <cxxhttp/http.h>

namespace cxxhttp {
namespace httpd {
/* HTTP servlet container.
 * @transport What transport the servlet is for, e.g. transport::tcp.
 *
 * This contains all the data needed to set up a subprocessor for the default
 * server processor type. This consists of regexen to match against a requested
 * resource and the method invoked, as well as a handler for a succsseful match.
 *
 * For advanced usage, it is possible to provide data or content negotiation.
 *
 * This alias makes initialisation slightly more convenient, by removing the
 * need to specify the session type. Instead, this just uses the session type
 * for a default HTTP server.
 */
template <class transport>
using servlet =
    http::servlet<transport, typename http::server<transport>::session>;

namespace cli {
using efgy::cli::option;

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
                  std::set<http::server<transport> *> &servers =
                      efgy::global<std::set<http::server<transport> *>>(),
                  service &service = efgy::global<cxxhttp::service>(),
                  std::set<servlet<transport> *> &servlets =
                      efgy::global<std::set<servlet<transport> *>>()) {
  bool rv = false;

  for (net::endpointType<transport> endpoint : lookup) {
    http::server<transport> *s =
        new http::server<transport>(endpoint, servers, service);

    s->processor.servlets = servlets;

    rv = rv || true;
  }

  return rv;
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
  return setup(net::endpoint<transport::unix>(match[1]));
}

/* TCP HTTP server CLI option.
 *
 * The format is `http:(interface-address):(port)`. The server that is set up
 * will have all available servlets registered.
 */
static option TCP(
    "-{0,2}http:(.+):([0-9]+)", setupTCP,
    "listen for HTTP connections on the given host[1] and port[2]");

/* UNIX socket HTTP server CLI option.
 *
 * The format is `http:unix:(socket-file)`. The server that is set up will have
 * all available servlets registered.
 */
static option UNIX("-{0,2}http:unix:(.+)", setupUNIX,
                   "listen for HTTP connections on the given unix socket[1]");
}

namespace usage {
using efgy::cli::hint;

/* Create usage hint.
 * @transport Used to look up the correct set of servlets, e.g. transpor::tcp.
 *
 * This creates a brief textual summary of the available servlets, with the
 * regular expressions that are used to match them against incoming queries.
 *
 * @return A string with a summary of the available servlets.
 */
template <typename transport>
static std::string describe(void) {
  std::string rv;
  const auto &servlets = efgy::global<std::set<servlet<transport> *>>();
  for (const auto &servlet : servlets) {
    rv += " * " + servlet->methodx + " " + servlet->resourcex + "\n   " +
          servlet->description + "\n";
  }
  return rv;
}

/* TCP servlet hints.
 *
 * Enables a description of all available TCP servlets for the `--help` flag.
 */
static hint TCP("HTTP Endpoints (TCP)", describe<transport::tcp>);

/* UNIX socket servlet hints.
 *
 * Enables a description of all available UNIX servlets for the `--help` flag.
 */
static hint UNIX("HTTP Endpoints (UNIX)", describe<transport::unix>);
}
}
}

#endif

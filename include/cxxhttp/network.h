/* Basic network management building blocks.
 *
 * For all those little things that networking needs.
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
#if !defined(CXXHTTP_NETWORK_H)
#define CXXHTTP_NETWORK_H

#include <iostream>
#include <string>

#define ASIO_STANDALONE
#include <asio.hpp>

#include <ef.gy/cli.h>
#include <ef.gy/global.h>

namespace cxxhttp {
/* asio::io_service type.
 *
 * We declare this here so expicitly declare support for this.
 */
using service = asio::io_service;

// define USE_DEFAULT_IO_MAIN to use this main function, or just call it.
#if defined(USE_DEFAULT_IO_MAIN)
#define IO_MAIN_SPEC extern "C"
#else
#define IO_MAIN_SPEC static inline
#endif

/* Default IO main function.
 * @argc Argument count.
 * @argv Argument vector.
 *
 * Applies all arguments with the efgy::cli facilities, then (tries to) run an
 * ASIO I/O loop.
 *
 * @return 0 on success, -1 on failure.
 */
IO_MAIN_SPEC int main(int argc, char *argv[]) {
  efgy::cli::options opts(argc, argv);

  efgy::global<service>().run();

  return opts.matches == 0 ? -1 : 0;
}

namespace transport {
using tcp = asio::ip::tcp;
using unix = asio::local::stream_protocol;
}

namespace net {
/* Resolve UNIX socket address.
 * @socket The UNIX socket wrapper; ignored.
 *
 * This is currently not supported, so will just return a string to indicate
 * that it's a UNIX socket and not something else.
 *
 * @return A fixed string that says that this is a UNIX socket.
 */
static inline std::string address(const transport::unix::socket &socket) {
  return "[UNIX]";
}

/* Resolve TCP socket address.
 * @socket The socket to examine.
 *
 * Examines a TCP socket to resolve the host address on the other end.
 *
 * @return The address of whatever the socket is connected to.
 */
static inline std::string address(const transport::tcp::socket &socket) {
  try {
    return socket.remote_endpoint().address().to_string();
  } catch (...) {
    return "[UNAVAILABLE]";
  }
}

/* Endpoint type alias.
 * @transport The ASIO transport type, e.g. asio::ip::tcp.
 *
 * This alias is only here for readability's sake, by cutting down on dependant
 * and nested types.
 */
template <typename transport>
using endpointType = typename transport::endpoint;

/* ASIO endpoint wrapper.
 * @transport The ASIO transport type, e.g. asio::ip::tcp.
 *
 * This is a wrapper class for ASIO transport types, primarily to make the
 * interface between them slightly more similar for the setup stages.
 */
template <typename transport = transport::unix>
class endpoint : public std::array<endpointType<transport>, 1> {
 public:
  /* Construct with socket name.
   * @pSocket A UNIX socket address.
   *
   * Initialses the endpoint given a socket name. This merely forwards
   * construction to the base class.
   */
  endpoint(const std::string &pSocket)
      : std::array<endpointType<transport>, 1>{{pSocket}} {}
};

/* ASIO TCP endpoint wrapper.
 *
 * The primary reason for specialising this wrapper is to allow for a somewhat
 * more involved target lookup than for sockets.
 */
template <>
class endpoint<transport::tcp> {
 public:
  /* Transport type.
   *
   * This type alias is for symmetry with the generic template.
   */
  using transport = cxxhttp::transport::tcp;

  /* Endpoint type.
   *
   * Endpoint type for a TCP connection.
   */
  using endpointType = transport::endpoint;

  /* DNS resolver type.
   *
   * A shorthand type name for the ASIO TCP DNS resolver.
   */
  using resolver = transport::resolver;

  /* Construct with host and port.
   * @pHost The target host for the endpoint.
   * @pPort The target port for the endpoint. Can be a service name.
   * @pService IO service, for use when resolving host and port names.
   *
   * Remembers a socket's host and port, but does not look them up just yet.
   * with() does that.
   */
  endpoint(const std::string &pHost, const std::string &pPort,
           cxxhttp::service &pService = efgy::global<cxxhttp::service>())
      : host(pHost), port(pPort), service(pService) {}

  /* Get first iterator for DNS resolution.
   *
   * To make DNS resolution work all nice and shiny with C++11 for loops.
   *
   * @return An interator pointing to the start of resolved endpoints.
   */
  resolver::iterator begin(void) const {
    return resolver(service).resolve(resolver::query(host, port));
  }

  /* Get final iterator for DNS resolution.
   *
   * Basically an empty iterator, which is how ASIO works here.
   *
   * @return An iterator pointing past the final element of resolved endpoints.
   */
  resolver::iterator end(void) const { return resolver::iterator(); }

 protected:
  /* Host name.
   *
   * Passed into the constructor. Can be an IP address in string form, in which
   * case the resolver will still get it right.
   */
  const std::string host;

  /* Port number.
   *
   * Can be a service name, in which case the resolver will look that up later.
   */
  const std::string port;

  /* IO service reference.
   *
   * Used by with() to create a DNS resolver.
   */
  cxxhttp::service &service;
};

/* Basic asynchronous connection wrapper
 * @requestProcessor The class of something that can handle requests.
 *
 * Contains all the data relating to a particular connection - either for a
 * server, or a client.
 */
template <typename requestProcessor>
class connection {
 public:
  /* The session type.
   *
   * The request processor needs to know about the kinds of sessions it's
   * dealing with, so we grab that type here because we're keeping track of the
   * sessions ourselves, below.
   */
  using session = typename requestProcessor::session;

  /* Initialise with IO service
   * @pio IO service to use.
   * @logfile A stream to write log messages to.
   *
   * Default constructor which binds an IO service and sets up a new processor.
   */
  connection(service &pio, std::ostream &logfile)
      : io(pio), log(logfile), pending(true) {}

  /* Request processor instance
   *
   * Used to generate replies for incoming queries.
   */
  requestProcessor processor;

  /* IO service
   *
   * Bound in the constructor to an asio.hpp IO service which handles
   * asynchronous connections.
   */
  service &io;

  /* Log stream
   *
   * This is a standard output stream to send log data to. Log data is written
   * by the session code, so that code determines the format of log lines.
   */
  std::ostream &log;

  /* Are there pending connections?
   *
   * Initially set to true, but reset to false if, for whatever reason, there
   * are no more pending connections for the given session.
   */
  bool pending;

  /* Active sessions.
   *
   * The sessions that are currently active. This list is maintained using a
   * beaon in the session object.
   */
  efgy::beacons<session> sessions;

  /* Is this connection still active?
   *
   * A connection is considered active if it either has at least one session
   * pending, or if it has the `pending` flag set.
   *
   * If a connection is inactive, it can be then be garbage-collected.
   *
   * @return Whether the connection is still active or not.
   */
  bool active(void) const { return pending || sessions.size() > 0; }
};
}
}

#endif

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
 * @session The session type. We need this as they're registered.
 * @requestProcessor The class of something that can handle requests.
 *
 * Contains all the data relating to a particular connection - either for a
 * server, or a client.
 */
template <typename session, typename requestProcessor>
class connection {
 public:
  using transport = typename session::transportType;

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

  /* Initialise with IO service
   * @endpoint Where to connect to, or listen on.
   * @pio IO service to use.
   * @pConnections The root of the connection set to register with.
   *
   * Default constructor which binds an IO service and sets up a new processor.
   */
  connection(endpointType<transport> &endpoint,
             efgy::beacons<connection> &pConnections =
                 efgy::global<efgy::beacons<connection>>(),
             service &pio = efgy::global<service>())
      : io(pio),
        pending(true),
        acceptor(pio),
        target(endpoint),
        beacon(*this, pConnections) {
    if (processor.listen()) {
      acceptor.open(endpoint.protocol());
      acceptor.bind(endpoint);
      acceptor.listen();
      startAccept();
    } else {
      startConnect();
    }
  }

  /* Reuse idle connection, or create new one.
   * @endpoint Where to connect to, or listen on.
   * @pio IO service to use.
   * @pConnections The root of the connection set to register with.
   *
   * This will scan through all connections to find one that is either idle and
   * return that one, or one that has the same paramters to tag along to, or it
   * will create an entirely new one.
   */
  static connection &get(endpointType<transport> &endpoint,
                         efgy::beacons<connection> &pConnections =
                             efgy::global<efgy::beacons<connection>>(),
                         service &pio = efgy::global<service>()) {
    for (auto &c : pConnections) {
      if (&pio == &(c->io)) {
        if (c->idle()) {
          c->pending = true;
          c->target = endpoint;
          c->startConnect();
          return *c;
        } else if (c->target == endpoint) {
          return *c;
        }
      }
    }

    return *(new connection(endpoint, pConnections, pio));
  }

  /* Is this session idle?
   *
   * Used when trying to find a session to reuse. This determines whether the
   * session can just be reused directly, without further processing.
   *
   * @return Whether the session can be reused.
   */
  bool idle(void) const {
    if (!pending) {
      if (sessions.size() == 0) {
        return true;
      }

      for (const auto &s : sessions) {
        if (!s->free) {
          return false;
        }
      }

      return true;
    }

    return false;
  }

  /* Query local endpoint.
   *
   * Queries and returns the local endpoint that a server is bound to.
   * Particularly useful for binding servers to a random port and then trying to
   * use them.
   *
   * @return The local endpoint for this server.
   */
  typename transport::endpoint endpoint(void) {
    return acceptor.local_endpoint();
  }

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

 protected:
  /* Socket acceptor
   *
   * This is the acceptor which has been bound to the socket specified in the
   * constructor.
   */
  typename transport::acceptor acceptor;

  /* Target endpoint.
   *
   * This is where we want to connect to.
   */
  endpointType<transport> target;

  /* Connection beacon.
   *
   * Registration in this set is handled automatically in the constructor.
   */
  efgy::beacon<connection> beacon;

  /* Accept the next incoming connection
   * @newSession An optional session to reuse.
   *
   * This function creates a new, blank session to handle the next incoming
   * request.
   */
  void startAccept(session *newSession = 0) {
    if (newSession == 0) {
      newSession = getSession();
    }

    acceptor.async_accept(newSession->socket.lowest_layer(),
                          [newSession, this](const std::error_code &error) {
                            handleAccept(newSession, error);
                          });
  }

  /* Connect to the socket.
   * @newSession An optional session to reuse.
   *
   * This function creates a new, blank session and attempts to connect to the
   * given socket.
   */
  void startConnect(session *newSession = 0) {
    if (newSession == 0) {
      newSession = getSession();
    }

    newSession->socket.lowest_layer().async_connect(
        target, [newSession, this](const std::error_code &error) {
          handleConnect(newSession, error);
        });
  }

  /* Handle next incoming connection
   * @newSession The blank session object that was created by startAccept().
   * @error Describes any error condition that may have occurred.
   *
   * Called by asio.hpp when a new inbound connection has been accepted; this
   * will make the session parse the incoming request and dispatch it to the
   * request processor specified as a template argument.
   */
  void handleAccept(session *newSession, const std::error_code &error) {
    if (error) {
      startAccept(newSession);
    } else {
      newSession->start();
      startAccept();
    }
  }

  /* Handle new connection
   * @newSession The blank session object that was created by startConnect().
   * @error Describes any error condition that may have occurred.
   *
   * Called by asio.hpp when a new outbound connection has been accepted; this
   * allows the session object to begin interacting with the new session.
   */
  void handleConnect(session *newSession, const std::error_code &error) {
    pending = false;

    if (error) {
      delete newSession;
    } else {
      newSession->start();
    }
  }

  /* Get a free session.
   *
   * Steps through all sessions to obtain a free one, If no sessions are free,
   * this will instead return a brand new one.
   *
   * This allows recycling sessions, which in turn means we don't have to do
   * ugly things like kill sessions ourselves.
   *
   * @return A free session, or null.
   */
  session *getSession(void) {
    for (auto &sess : sessions) {
      if (sess->free) {
        sess->free = false;
        return sess;
      }
    }

    return new session(*this);
  }
};
}
}

#endif

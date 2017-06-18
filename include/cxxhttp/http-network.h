/* asio.hpp-assisted HTTP protocol handling
 *
 * An asynchronous HTTP server implementation using asio.hpp and std::regex.
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
#if !defined(CXXHTTP_HTTP_NETWORK_H)
#define CXXHTTP_HTTP_NETWORK_H

#include <cxxhttp/network.h>

#include <cxxhttp/http-flow.h>
#include <cxxhttp/http-processor.h>

namespace cxxhttp {
namespace http {
/* Session wrapper
 * @transport The connection type, e.g. transport::tcp.
 * @requestProcessor The functor class to handle requests.
 *
 * Used by the server to keep track of all the data associated with a single,
 * asynchronous client connection.
 */
template <typename transport, typename requestProcessor>
class session : public sessionData {
 public:
  /* Transport type alias.
   *
   * Referenced here so we don't have to specify it elsewhere.
   */
  using transportType = transport;

  /* Connection type.
   *
   * This is the type of the server or client that the session is being served
   * on; used when instantiating a session, as we need to use some of the data
   * the server or client may have to offer.
   */
  using connectionType = net::connection<session, requestProcessor>;

  /* Connection socket type.
   *
   * The type of socket we're talking to, which is the transport's socket type.
   */
  using socketType = typename transport::socket;

  /* HTTP Coordinator type.
   *
   * We need one instance of this to do the actual work of talking with the
   * other end of this connection.
   */
  using flowType = http::flow<requestProcessor, socketType>;

  /* Connection instance
   *
   * A reference to the server or client that this session belongs to and was
   * spawned from. Used to process requests and potentially for general
   * maintenance.
   */
  connectionType &connection;

  /* HTTP Coordinator.
   *
   * Provides flow control for our HTTP session.
   */
  flowType flow;

  /* Stream socket
   *
   * This is the asynchronous I/O socket that is used to communicate with the
   * client.
   *
   * We have a reference to this here so that the session can accept incoming
   * connections into this socket.
   */
  socketType &socket;

  /* Construct with I/O connection
   * @pConnection The connection instance this session belongs to.
   *
   * Constructs a session with the given asynchronous connection.
   */
  session(connectionType &pConnection)
      : connection(pConnection),
        flow(connection.processor, connection.io, *this),
        socket(flow.inputConnection),
        beacon(*this, connection.sessions) {}

  /* Destructor.
   *
   * Closes the socket, cancels all remaining requests and sets the status to
   * stShutdown.
   */
  ~session(void) {
    if (!free) {
      recycle();
    }
  }

  /* Start processing.
   *
   * Starts processing the incoming request.
   */
  void start(void) { flow.start(); }

  /* Recycle session.
   *
   * Forwards to `flow.recycle()`, and should only be used by the connection
   * handler and our destructor.
   */
  void recycle(void) { flow.recycle(); }

 protected:
  /* Session beacon.
   *
   * We want to keep track of all of the sessions, so that we can do stats over
   * the lot of them.
   */
  efgy::beacon<session> beacon;
};

/* HTTP server template.
 * @transport Transport type for the server.
 *
 * This is a template for an HTTP server, based on net::server and using the
 * server processor.
 */
template <class transport>
using server =
    net::connection<session<transport, processor::server>, processor::server>;

/* HTTP client template.
 * @transport Transport type for the server.
 *
 * This is a template for an HTTP client, based on net::client and using the
 * client processor.
 */
template <class transport>
using client =
    net::connection<session<transport, processor::client>, processor::client>;
}
}

#endif

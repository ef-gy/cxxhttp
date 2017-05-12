/* asio.hpp Generic Server
 *
 * A generic asynchronous server template using asio.hpp.
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
#if !defined(CXXHTTP_SERVER_H)
#define CXXHTTP_SERVER_H

#include <cxxhttp/network.h>

namespace cxxhttp {
namespace net {
/* Basic asynchronous server wrapper
 * @transport The socket class, e.g. asio::ip::tcp
 * @requestProcessor The functor class to handle requests.
 *
 * Contains the code that accepts incoming requests and dispatches sessions to
 * process these requests asynchronously.
 */
template <typename transport, typename requestProcessor,
          template <typename, typename> class sessionTemplate>
class server : public connection<requestProcessor> {
 public:
  using connection = net::connection<requestProcessor>;
  using session = sessionTemplate<transport, requestProcessor>;

  /* Initialise with IO service
   * @endpoint Endpoint for the socket to bind.
   * @pio IO service to use.
   * @logfile A stream to write log messages to.
   *
   * Default constructor which binds an IO service to a socket endpoint that was
   * passed in. The socket is bound asynchronously.
   */
  server(endpointType<transport> &endpoint,
         service &pio = efgy::global<service>(),
         std::ostream &logfile = std::cout)
      : connection(pio, logfile), acceptor(pio, endpoint) {
    startAccept();
  }

 protected:
  /* Accept the next incoming connection
   *
   * This function creates a new, blank session to handle the next incoming
   * request.
   */
  void startAccept(void) {
    session *newSession = new session(*this);
    acceptor.async_accept(newSession->socket,
                          [newSession, this](const std::error_code &error) {
                            handleAccept(newSession, error);
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
    if (!error) {
      newSession->start();
    } else {
      delete newSession;
    }

    startAccept();
  }

  /* Socket acceptor
   *
   * This is the acceptor which has been bound to the socket specified in the
   * constructor.
   */
  typename transport::acceptor acceptor;
};
}
}

#endif

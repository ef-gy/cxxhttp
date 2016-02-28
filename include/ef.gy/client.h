/**\file
 * \brief asio.hpp Generic Client
 *
 * A generic asynchronous client template using asio.hpp.
 *
 * \copyright
 * This file is part of the libefgy project, which is released as open source
 * under the terms of an MIT/X11-style licence, described in the COPYING file.
 *
 * \see Project Documentation: https://ef.gy/documentation/libefgy
 * \see Project Source Code: https://github.com/ef-gy/libefgy
 * \see Licence Terms: https://github.com/ef-gy/libefgy/blob/master/COPYING
 */

#if !defined(EF_GY_CLIENT_H)
#define EF_GY_CLIENT_H

#include <memory>

#include <ef.gy/network.h>

namespace efgy {
namespace net {
/**\brief Basic asynchronous client wrapper
 *
 * Contains code that connects to a given endpoint and establishes a session for
 * the duration of that connection.
 *
 * \tparam base             The socket class, e.g. asio::ip::tcp
 * \tparam requestProcessor The functor class to handle requests.
 */
template <typename base, typename requestProcessor,
          template <typename, typename> class sessionTemplate>
class client : public connection<requestProcessor> {
public:
  using connection = connection<requestProcessor>;
  using session = sessionTemplate<base, requestProcessor>;

  /**\brief Initialise with IO service
   *
   * Default constructor which binds an IO service to a socket endpoint that was
   * passed in. The socket is bound asynchronously.
   *
   * \param[in]  endpoint Endpoint for the socket to bind.
   * \param[out] pio      IO service to use.
   * \param[out] logfile  A stream to write log messages to.
   */
  client(typename base::endpoint &endpoint,
         io::service &pio = io::service::common(),
         std::ostream &logfile = std::cout)
      : connection(pio, logfile), target(endpoint) {
    startConnect();
  }

protected:
  /**\brief Connect to the socket.
   *
   * This function creates a new, blank session and attempts to connect to the
   * given socket.
   */
  void startConnect(void) {
    std::shared_ptr<session> newSession = (new session(*this))->self;
    newSession->socket.async_connect(
        target, [newSession, this](const std::error_code &error) {
          handleConnect(newSession, error);
        });
  }

  /**\brief Handle new connection
   *
   * Called by asio.hpp when a new outbound connection has been accepted; this
   * allows the session object to begin interacting with the new session.
   *
   * \param[in] newSession The blank session object that was created by
   *                       startConnect().
   * \param[in] error      Describes any error condition that may have occurred.
   */
  void handleConnect(std::shared_ptr<session> newSession,
                     const std::error_code &error) {
    if (!error) {
      newSession->start();
    } else {
      newSession->self.reset();
    }
  }

  typename base::endpoint target;
};
}
}

#endif

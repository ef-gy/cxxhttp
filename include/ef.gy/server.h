/**\file
 * \brief asio.hpp Generic Server
 *
 * A generic asynchronous server template using asio.hpp.
 *
 * \copyright
 * This file is part of the libefgy project, which is released as open source
 * under the terms of an MIT/X11-style licence, described in the COPYING file.
 *
 * \see Project Documentation: https://ef.gy/documentation/libefgy
 * \see Project Source Code: https://github.com/ef-gy/libefgy
 * \see Licence Terms: https://github.com/ef-gy/libefgy/blob/master/COPYING
 */

#if !defined(EF_GY_SERVER_H)
#define EF_GY_SERVER_H

#include <memory>

#include <ef.gy/network.h>

namespace efgy {
namespace net {
/**\brief Basic asynchronous server wrapper
 *
 * Contains the code that accepts incoming requests and dispatches sessions to
 * process these requests asynchronously.
 *
 * \tparam base             The socket class, e.g. asio::ip::tcp
 * \tparam requestProcessor The functor class to handle requests.
 */
template <typename base, typename requestProcessor,
          template <typename, typename> class sessionTemplate>
class server : public connection<requestProcessor> {
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
  server(typename base::endpoint &endpoint,
         io::service &pio = io::service::common(),
         std::ostream &logfile = std::cout)
      : connection(pio, logfile), acceptor(pio.get(), endpoint) {
    startAccept();
  }

protected:
  /**\brief Accept the next incoming connection
   *
   * This function creates a new, blank session to handle the next incoming
   * request.
   */
  void startAccept(void) {
    std::shared_ptr<session> newSession = (new session(*this))->self;
    acceptor.async_accept(newSession->socket,
                          [newSession, this](const std::error_code &error) {
                            handleAccept(newSession, error);
                          });
  }

  /**\brief Handle next incoming connection
   *
   * Called by asio.hpp when a new inbound connection has been accepted; this
   * will make the session parse the incoming request and dispatch it to the
   * request processor specified as a template argument.
   *
   * \param[in] newSession The blank session object that was created by
   *                       startAccept().
   * \param[in] error      Describes any error condition that may have occurred.
   */
  void handleAccept(std::shared_ptr<session> newSession,
                    const std::error_code &error) {
    if (!error) {
      newSession->start();
    } else {
      newSession->self.reset();
    }

    startAccept();
  }

  /**\brief Socket acceptor
   *
   * This is the acceptor which has been bound to the socket specified in the
   * constructor.
   */
  typename base::acceptor acceptor;
};
}
}

#endif

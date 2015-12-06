/**\file
 * \brief asio.hpp Generic Client
 *
 * A generic asynchronous client template using asio.hpp.
 *
 * \copyright
 * Copyright (c) 2012-2015, ef.gy Project Members
 * \copyright
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * \copyright
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * \copyright
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * \see Project Documentation: http://ef.gy/documentation/libefgy
 * \see Project Source Code: https://github.com/ef-gy/libefgy
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

/* HTTP testing classes
 *
 * Provides a special fake transport and a record-only session instance, which
 * together allow testing of HTTP servlets.
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

#if !defined(CXXHTTP_HTTP_TEST_H)
#define CXXHTTP_HTTP_TEST_H

#include <cxxhttp/http.h>

namespace cxxhttp {
namespace transport {
/* Dummy tansport.
 *
 * A dummy transport class, which is only used to have a type that looks enough
 * like an ASIO transport type to make the server wrapper happy.
 */
class fake {
 public:
  /* Endpoint type stub.
   *
   * Not actually a real endpoint - we just need something to instantiate for
   * <net::server>.
   */
  using endpoint = int;

  /* Acceptor type stub.
   *
   * Not actually a real acceptor - we just need something to instantiate for
   * <net::server>.
   */
  using acceptor = int;
};
}

namespace http {
/* Dummy session.
 *
 * This is not a real session, though it looks enough like one to test HTTP
 * servlet handlers.
 */
template <class requestProcessor>
class session<transport::fake, requestProcessor> : public sessionData {
 public:
  /* Last status code.
   *
   * Set whenever a reply is sent back over the fake transport.
   */
  unsigned status;

  /* Last message.
   *
   * Set whenever a reply is sent back over the fake transport.
   */
  std::string message;

  /* Last header set.
   *
   * Set whenever a reply is sent back over the fake transport.
   */
  headers header;

  /* Stub connection.
   *
   * Since this isn't a real connection, it only contains an instance of the
   * processor.
   */
  struct {
    /* Processor instance.
     *
     * Can be used for end-to-end tests as needed.
     */
    requestProcessor processor;
  } connection;

  /* Record a reply.
   * @pStatus The status code to record.
   * @pMessage The message to record.
   * @pHeader The header set to record.
   *
   * Records the arguments in the corresponding member variables.
   *
   * Nothing is actually sent over any wires, because our transport is a fake
   * anyway.
   */
  void reply(int pStatus, const std::string &pMessage,
             const headers &pHeader = {}) {
    status = pStatus;
    message = pMessage;
    header = pHeader;
  }
};

/* Dummy session alias.
 *
 * Named as it is because that's the purpose of the dummy session: to record
 * the last data in-flight so it can be examined.
 */
using recorder = session<transport::fake, processor::server<transport::fake>>;
}
}

#endif

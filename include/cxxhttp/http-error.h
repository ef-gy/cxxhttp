/* HTTP error handling.
 *
 * Provides some abstractions to send back errors to clients.
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
#if !defined(CXXHTTP_HTTP_ERROR_H)
#define CXXHTTP_HTTP_ERROR_H

#include <set>

#include <cxxhttp/http-header.h>
#include <cxxhttp/http-status.h>
#include <cxxhttp/negotiate.h>

namespace cxxhttp {
namespace http {
/* Error reply handler.
 * @S The session type, which is needed to reply to something.
 *
 * `error` slightly simplifies and unifies error responses to clients, by
 * generating an appropriate response when an error needs to be sent.
 */
template <class S>
class error {
 public:
  /* Allowed methods.
   *
   * Sent as an `Allow` header, but only if it has been set.
   */
  std::set<std::string> allow;

  /* Construct with session.
   * @pSession Where to send things to.
   *
   * Keeps track of a session object, which is used when trying to send an error
   * to the client.
   */
  error(S &pSession) : session(pSession) {}

  /* Send error code to client.
   * @status The status code to send to the client. Should be an error code.
   *
   * This constructs and sends a simple error reply to the client.
   */
  void reply(unsigned status) const {
    std::string type = negotiate(session.inbound.get("Accept"),
                                 "text/markdown, text/plain;q=0.9");
    bool negotiationSuccess = !type.empty();

    if (type.empty()) {
      type = "text/markdown";
    }

    std::string body =
        "# " + statusLine::getDescription(status) +
        "\n\n"
        "An error occurred while processing your request. " +
        (negotiationSuccess ? "" : "Additionally, content type negotiation for "
                                   "this error page failed. ") +
        "That's all I know.\n";

    parser<headers> p{{{"Content-Type", type}}};

    for (const auto &m : allow) {
      p.append("Allow", m);
    }

    session.reply(status, body, p.header);
  }

 protected:
  /* The recorded session.
   *
   * Set in the constructor and used when sending replies.
   */
  S &session;
};
}
}

#endif

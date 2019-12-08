/* HTTP TRACE handler.
 *
 * This is an implementation of the TRACE method for the httpd.h server.
 *
 * Do note that there could be security concerns in leaving this enabled. In
 * particular, TRACE allows http-only cookies to be read out from JavaScript. I
 * do maintain that if your security relies on this, however, that you have
 * larger issues that need addressing.
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
#if !defined(CXXHTTP_HTTPD_TRACE_H)
#define CXXHTTP_HTTPD_TRACE_H

#include <cxxhttp/httpd.h>

namespace cxxhttp {
namespace httpd {
namespace trace {
/* HTTP TRACE implementation.
 * @session The session to reply to, with all the data we need.
 * @re The regex match fragments; ignored since we just respond to everything.
 *
 * Implements the HTTP TRACE method. This method responds with the more-or-less
 * verbatim request as a request body.
 *
 * This method is not really subject to negotiation, so we haven't specified any
 * of those.
 */
static void trace(http::sessionData &session, std::smatch &re) {
  session.reply(
      200, session.inboundRequest.assemble() + std::string(session.inbound),
      {{"Content-Type", "message/http"}});
}

/* TRACE location regex.
 *
 * We accept a TRACE method for literally any location.
 */
static const char *resource = ".*";

/* TRACE method regex.
 *
 * We accept TRACE queries only for the TRACE method. This is to make it not
 * interfere with other requests.
 */
static const char *method = "TRACE";

/* A description of how to use the HTTP TRACE method.
 *
 * The description in the original RFC is actually pretty good, so we just link
 * to that here. Do note that the original RFC doesn't call out the theoretical
 * security issues with getting to cookies in JS you normally couldn't.
 */
static const char *description =
    "See [RFC 2616, section 9.8]"
    "(https://tools.ietf.org/html/rfc2616#section-9.8).";

#if !defined(NO_DEFAULT_TRACE)
/* TRACE servlet.
 *
 * Sets up the servlet for the default HTTP processor.
 */
static http::servlet servlet(resource, trace, method, {}, description);
#endif
}  // namespace trace
}  // namespace httpd
}  // namespace cxxhttp

#endif

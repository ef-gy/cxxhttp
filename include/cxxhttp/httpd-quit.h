/* HTTP /quit handler.
 *
 * A simple example handler that allows the server to be shut down by accessing
 * the /quit resource with a GET request.
 *
 * This is highly non-standard, and probably a bad idea in production. So...
 * just say "no". Although this IS only enabled on UNIX sockets if you include
 * this file, so there's that.
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
#if !defined(CXXHTTP_HTTPD_QUIT_H)
#define CXXHTTP_HTTPD_QUIT_H

#include <cxxhttp/httpd.h>

namespace cxxhttp {
namespace httpd {
namespace quit {
/* Sample request handler for /quit
 *
 * When this handler is invoked, it stops the ASIO IO handler (after replying,
 * maybe...).
 *
 * Having this on your production server in this exact way is PROBABLY a really
 * bad idea, unless you gate it in an upstream forward proxy. Or you have some
 * way of automatically respawning your server. Or both.
 *
 * It would probably be OK to have this on by default on unix sockets, eeing as
 * you need to be logged onto the system to access those...
 *
 * @session The HTTP session to answer on.
 * @returns true (always, as we always reply).
 */
template <class transport>
static bool quit(typename http::server<transport>::session &session,
                 std::smatch &) {
  session.reply(200, "Good-Bye, Cruel World!");

  session.connection.io.stop();

  return true;
}

static const char *resource = "/quit";

#if !defined(NO_DEFAULT_QUIT)
static httpd::servlet<transport::unix> UNIX(resource, quit<transport::unix>);
#endif
}
}
}

#endif

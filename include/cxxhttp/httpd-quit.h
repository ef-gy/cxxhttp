/**\file
 *
 * \copyright
 * This file is part of the cxxhttp project, which is released as open source
 * under the terms of an MIT/X11-style licence, described in the COPYING file.
 *
 * \see Project Documentation: https://ef.gy/documentation/cxxhttp
 * \see Project Source Code: https://github.com/ef-gy/cxxhttp
 * \see Licence Terms: https://github.com/ef-gy/cxxhttp/blob/master/COPYING
 */

#if !defined(CXXHTTP_HTTPD_QUIT_H)
#define CXXHTTP_HTTPD_QUIT_H

#include <cxxhttp/httpd.h>

namespace cxxhttp {
namespace httpd {
namespace quit {
/**\brief Sample request handler for /quit
 *
 * When this handler is invoked, it stops the ASIO IO handler (after replying,
 * maybe...).
 *
 * \note Having this on your production server in this exact way is PROBABLY a
 *       really bad idea, unless you gate it in an upstream forward proxy. Or
 *       you have some way of automatically respawning your server. Or both.
 *
 * \note It would probably be OK to have this on by default on unix sockets,
 *       seeing as you need to be logged onto the system to access those...
 *
 * \param[out] session The HTTP session to answer on.
 *
 * \returns true (always, as we always reply).
 */
template <class transport>
static bool quit(typename net::http::server<transport>::session &session,
                 std::smatch &) {
  session.reply(200, "Good-Bye, Cruel World!");

  session.connection.io.get().stop();

  return true;
}

static std::string regex = "/quit";

#if !defined(NO_DEFAULT_QUIT)
static httpd::servlet<asio::local::stream_protocol>
    UNIX(regex, quit<asio::local::stream_protocol>);
#endif
}
}
}

#endif

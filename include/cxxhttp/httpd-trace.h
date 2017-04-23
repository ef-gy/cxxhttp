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
template <class transport>
static bool trace(typename http::server<transport>::session &session,
                  std::smatch &re) {
  std::string message =
      session.method + " " + session.resource + " " + session.protocol + "\r\n";

  for (const auto &h : session.header) {
    message += h.first + ": " + h.second + "\r\n";
  }

  session.reply(200,
                {
                    {"Content-Type", "message/http"},
                },
                message);

  return true;
}

static const char *regex = ".*";

#if !defined(NO_DEFAULT_TRACE)
static httpd::servlet<transport::tcp> TCP(regex, trace<transport::tcp>,
                                          "TRACE");
static httpd::servlet<transport::unix> UNIX(regex, trace<transport::unix>,
                                            "TRACE");
#endif
}
}
}

#endif

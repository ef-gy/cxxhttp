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

#if !defined(CXXHTTP_HTTPD_TRACE_H)
#define CXXHTTP_HTTPD_TRACE_H

#include <cxxhttp/httpd.h>

namespace cxxhttp {
namespace httpd {
namespace trace {
template <class transport>
static bool trace(typename net::http::server<transport>::session &session,
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

#if !defined(NO_DEFAULT_TRACE)
static httpd::servlet<asio::ip::tcp> TCP(".*", trace<asio::ip::tcp>, "TRACE");
static httpd::servlet<asio::local::stream_protocol>
    UNIX(".*", trace<asio::local::stream_protocol>, "TRACE");
#endif
}
}
}

#endif

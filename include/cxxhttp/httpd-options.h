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

#if !defined(CXXHTTP_HTTPD_OPTIONS_H)
#define CXXHTTP_HTTPD_OPTIONS_H

#include <cxxhttp/httpd.h>

namespace cxxhttp {
namespace httpd {
namespace options {
template <class transport>
static bool options(typename net::http::server<transport>::session &session,
                    std::smatch &re) {
  std::string text = "# Applicable Resource Options\n\n";
  std::set<std::string> methods {};
  std::string allow = "";
  const std::string full = re[0];

  for (const auto &servlet :
       set<transport, servlet<transport>>::common().servlets) {
    std::regex rx(servlet->regex);
    std::regex mx(servlet->methods);

    if ((full == "*") || std::regex_match(full, rx)) {
      text += "* _" + servlet->methods + "_ `" + servlet->regex + "`\n";
      for (const auto &m : net::http::method) {
        if (std::regex_match(m, mx)) {
          methods.insert(m);
        }
      }
    }
  }

  for (const auto &m : methods) {
    allow += (allow == "" ? "" : ",") + m;
  }

  session.reply(200, {
    {"Content-Type", "text/markdown; charset=UTF-8"},
    {"Allow", allow},
  }, text);

  return true;
}

#if !defined(NO_DEFAULT_OPTIONS)
static httpd::servlet<asio::ip::tcp> TCP(
    ".*", options<asio::ip::tcp>, "OPTIONS");
static httpd::servlet<asio::local::stream_protocol>
    UNIX(".*", options<asio::local::stream_protocol>,
    "OPTIONS");
#endif
}
}
}

#endif

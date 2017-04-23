/* HTTP OPTIONS extension.
 *
 * This file implements a generic version of the OPTIONS method, for use with
 * the httpd.h server and servlet implementation.
 *
 * This implementation also has a minor extension, whereby the valid and
 * applicable servlets are provided in markdown.
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
#if !defined(CXXHTTP_HTTPD_OPTIONS_H)
#define CXXHTTP_HTTPD_OPTIONS_H

#include <cxxhttp/httpd.h>

namespace cxxhttp {
namespace httpd {
namespace options {
template <class transport>
static bool options(typename net::http::server<transport>::session &session,
                    std::smatch &re) {
  std::string text = "# Applicable Resource Processors\n\n";
  std::set<std::string> methods{};
  std::string allow = "";
  const std::string full = re[0];

  for (const auto &servlet : efgy::registered<servlet<transport>>::common()) {
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

  headers header{{"Content-Type", "text/markdown; charset=UTF-8"}};

  for (const auto &m : methods) {
    append(header, "Allow", m);
  }

  session.reply(200, header, text);

  return true;
}

static const char *regex = "^\\*|/.*";

#if !defined(NO_DEFAULT_OPTIONS)
static httpd::servlet<transport::tcp> TCP(regex, options<transport::tcp>,
                                          "OPTIONS");
static httpd::servlet<transport::unix> UNIX(regex, options<transport::unix>,
                                            "OPTIONS");
#endif
}
}
}

#endif

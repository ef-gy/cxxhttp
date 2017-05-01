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
static bool options(typename http::server<transport>::session &session,
                    std::smatch &re) {
  std::string text = "# Applicable Resource Processors\n\n";
  std::set<std::string> methods{};
  std::string allow = "";
  const std::string full = re[0];

  const auto &servlets = efgy::global<std::set<servlet<transport> *>>();
  for (const auto &servlet : servlets) {
    std::regex rx(servlet->resourcex);
    std::regex mx(servlet->methodx);

    if ((full == "*") || std::regex_match(full, rx)) {
      text += "* _" + servlet->methodx + "_ `" + servlet->resourcex + "`\n" +
              "  " + servlet->description + "\n";
      for (const auto &m : http::method) {
        if (std::regex_match(m, mx)) {
          methods.insert(m);
        }
      }
    }
  }

  http::parser<http::headers> p{
      {{"Content-Type", "text/markdown; charset=UTF-8"}}};

  for (const auto &m : methods) {
    p.append("Allow", m);
  }

  session.reply(200, p.header, text);

  return true;
}

static const char *resource = "^\\*|/.*";
static const char *method = "OPTIONS";

#if !defined(NO_DEFAULT_OPTIONS)
static httpd::servlet<transport::tcp> TCP(resource, options<transport::tcp>,
                                          method);
static httpd::servlet<transport::unix> UNIX(resource, options<transport::unix>,
                                            method);
#endif
}
}
}

#endif

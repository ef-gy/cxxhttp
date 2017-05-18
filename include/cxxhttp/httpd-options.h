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
/* HTTP OPTIONS method implementation.
 * @transport Transport type of the session.
 * @session The HTTP session to reply to.
 * @re Parsed resource match set.
 *
 * This function implements the HTTP OPTIONS method, which informs cliets of the
 * methods available at a given resource.
 */
template <class transport>
static void options(typename http::server<transport>::session &session,
                    std::smatch &re) {
  std::string text = "# Applicable Resource Processors\n\n";
  std::set<std::string> methods{};
  const std::string full = re[0];

  for (const auto &servlet : efgy::global<std::set<servlet<transport> *>>()) {
    if ((full == "*") || std::regex_match(full, servlet->resource)) {
      text += servlet->describe();
      for (const auto &m : http::method) {
        if (std::regex_match(m, servlet->method)) {
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
}

/* HTTP OPTIONS location regex.
 *
 * We allow pretty much everything, but we're explicitly saying that a * is
 * acceptable as well.
 */
static const char *resource = "^\\*|/.*";

/* HTTP OPTIONS method regex.
 *
 * We only care for the OPTIONS method. Since we already allow every resource
 * location, this prevents this handler from interfering with other servlets.
 */
static const char *method = "OPTIONS";

#if !defined(NO_DEFAULT_OPTIONS)
/* HTTP OPTIONS servlet on TCP.
 *
 * Sets up a servlet to respond to OPTIONS queries on TCP sockets.
 */
static httpd::servlet<transport::tcp> TCP(resource, options<transport::tcp>,
                                          method);

/* HTTP OPTIONS servlet on UNIX sockets.
 *
 * Sets up a servlet for OPTIONS queries on UNIX sockets.
 */
static httpd::servlet<transport::unix> UNIX(resource, options<transport::unix>,
                                            method);
#endif
}
}
}

#endif

/* asio.hpp HTTP server.
 *
 * This file implements an actual HTTP server with a simple servlet capability
 * in top of http.h.
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
#if !defined(CXXHTTP_HTTPD_H)
#define CXXHTTP_HTTPD_H

#include <ef.gy/cli.h>
#include <ef.gy/global.h>

#include <cxxhttp/http.h>

namespace cxxhttp {
namespace httpd {
template <class processor, class servlet>
void apply(processor &proc, std::set<servlet *> &servlets) {
  for (const auto &s : servlets) {
    proc.add(s->regex, s->handler, s->methods, s->negotiations);
  }
}

template <class transport>
class servlet {
 public:
  servlet(const std::string &pRegex,
          std::function<bool(typename net::http::server<transport>::session &,
                             std::smatch &)>
              pHandler,
          const std::string &pMethods = "GET", const headers pNegotiations = {},
          const std::string &pDescription = "no description available.",
          std::set<servlet *> &pSet = efgy::global<std::set<servlet *>>())
      : regex(pRegex),
        methods(pMethods),
        handler(pHandler),
        negotiations(pNegotiations),
        description(pDescription),
        root(pSet) {
    root.insert(this);
  }

  ~servlet(void) { root.erase(this); }

  const std::string regex;
  const std::string methods;
  const std::function<bool(typename net::http::server<transport>::session &,
                           std::smatch &)>
      handler;
  const headers negotiations;
  const std::string description;

 protected:
  std::set<servlet *> &root;
};

template <class transport>
static std::size_t setup(net::endpoint<transport> lookup,
                         io::service &service = io::service::common()) {
  return lookup.with(
      [&service](typename transport::endpoint &endpoint) -> bool {
        net::http::server<transport> *s =
            new net::http::server<transport>(endpoint, service);

        apply(s->processor, efgy::global<std::set<servlet<transport> *>>());

        return true;
      });
}

namespace transport {
using tcp = net::transport::tcp;
using unix = net::transport::unix;
}

namespace cli {
using efgy::cli::option;

static option socket(
    "-{0,2}http:unix:(.+)",
    [](std::smatch &m) -> bool {
      return setup(net::endpoint<transport::unix>(m[1])) > 0;
    },
    "Listen for HTTP connections on the given unix socket[1].");

static option tcp(
    "-{0,2}http:(.+):([0-9]+)",
    [](std::smatch &m) -> bool {
      return setup(net::endpoint<transport::tcp>(m[1], m[2])) > 0;
    },
    "Listen for HTTP connections on the given host[1] and port[2].");
}

namespace usage {
using efgy::cli::hint;

template <typename transport>
static std::string print(void) {
  std::string rv = "";
  for (const auto &servlet : efgy::global<std::set<servlet<transport> *>>()) {
    rv += " " + servlet->methods + " " + servlet->regex + "\n";
  }
  return rv;
}

static hint tcp("HTTP Endpoints (TCP)", print<transport::tcp>);
static hint unix("HTTP Endpoints (UNIX)", print<transport::unix>);
}
}
}

#endif

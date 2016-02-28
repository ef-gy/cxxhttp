/**\file
 *
 * \copyright
 * This file is part of the libefgy project, which is released as open source
 * under the terms of an MIT/X11-style licence, described in the COPYING file.
 *
 * \see Project Documentation: https://ef.gy/documentation/libefgy
 * \see Project Source Code: https://github.com/ef-gy/libefgy
 * \see Licence Terms: https://github.com/ef-gy/libefgy/blob/master/COPYING
 */

#if !defined(EF_GY_HTTPD_H)
#define EF_GY_HTTPD_H

#include <ef.gy/http.h>

namespace efgy {
namespace httpd {

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

template <class transport> class servlet;

template <class transport, class servlet = servlet<transport>> class set {
public:
  static set &common(void) {
    static set s;
    return s;
  }

  template <class processor> set &apply(processor &proc) {
    for (const auto &s : servlets) {
      proc.add(s->regex, s->handler, s->methods);
    }
    return *this;
  }

  set &add(servlet &o) {
    servlets.insert(&o);
    return *this;
  }

  set &remove(servlet &o) {
    servlets.erase(&o);
    return *this;
  }

  std::set<servlet *> servlets;
};

template <class transport> class servlet {
public:
  servlet(const std::string pRegex,
          std::function<bool(typename net::http::server<transport>::session &,
                             std::smatch &)> pHandler,
          const std::string pMethods = "GET",
          set<transport, servlet> &pSet = set<transport, servlet>::common())
      : regex(pRegex), methods(pMethods), handler(pHandler), servlets(pSet) {
    servlets.add(*this);
  }

  ~servlet(void) { servlets.remove(*this); }

  const std::string regex;
  const std::string methods;
  const std::function<bool(typename net::http::server<transport>::session &,
                           std::smatch &)> handler;

protected:
  set<transport, servlet> &servlets;
};

template <class sock>
static std::size_t setup(net::endpoint<sock> lookup,
                         io::service &service = io::service::common()) {
  return lookup.with([&service](typename sock::endpoint &endpoint) -> bool {
    net::http::server<sock> *s = new net::http::server<sock>(endpoint, service);

    set<sock>::common().apply(s->processor);

    return true;
  });
}

static cli::option socket("-{0,2}http:unix:(.+)", [](std::smatch &m) -> bool {
  return setup(net::endpoint<asio::local::stream_protocol>(m[1])) > 0;
}, "Listen for HTTP connections on the given unix socket[1].");

static cli::option tcp("-{0,2}http:(.+):([0-9]+)", [](std::smatch &m) -> bool {
  return setup(net::endpoint<asio::ip::tcp>(m[1], m[2])) > 0;
}, "Listen for HTTP connections on the given host[1] and port[2].");

namespace usage {
template <typename transport> static std::string print(void) {
  std::string rv = "";
  for (const auto &servlet :
       set<transport, servlet<transport>>::common().servlets) {
    rv += " " + servlet->methods + " " + servlet->regex + "\n";
  }
  return rv;
}

static cli::hint tcp("HTTP Endpoints (TCP)", print<asio::ip::tcp>);
static cli::hint unix("HTTP Endpoints (UNIX)",
                      print<asio::local::stream_protocol>);
}
}
}

#endif

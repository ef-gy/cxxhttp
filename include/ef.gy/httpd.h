/**\file
 *
 * \copyright
 * Copyright (c) 2012-2015, ef.gy Project Members
 * \copyright
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * \copyright
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * \copyright
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * \see Project Documentation: http://ef.gy/documentation/libefgy
 * \see Project Source Code: https://github.com/ef-gy/libefgy
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

  session.server.io.get().stop();

  return true;
}

template <class transport> class servlet;

template <class transport, class servlet = servlet<transport> > class set {
public:
  static set &common(void) {
    static set s;
    return s;
  }

  template <class processor> set &apply(processor &proc) {
    for (const auto &s : servlets) {
      proc.add(s->regex, s->handler);
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

protected:
  std::set<servlet *> servlets;
};

template <class transport> class servlet {
public:
  servlet(const std::string pRegex,
          std::function<bool(typename net::http::server<transport>::session &,
                             std::smatch &)> pHandler,
          set<transport, servlet> &pSet = set<transport, servlet>::common())
      : regex(pRegex), handler(pHandler), servlets(pSet) {
    servlets.add(*this);
  }

  ~servlet(void) { servlets.remove(*this); }

  const std::string regex;
  const std::function<bool(typename net::http::server<transport>::session &,
                           std::smatch &)> handler;

protected:
  set<transport, servlet> &servlets;
};

template <class sock>
static std::size_t setup(net::endpoint<sock> lookup,
                         io::service &service = io::service::common()) {
  return lookup.with([&service](typename sock::endpoint & endpoint)->bool {
    net::http::server<sock> *s = new net::http::server<sock>(endpoint, service);

    set<sock>::common().apply(s->processor);

    return true;
  });
}

static cli::option socket("http:unix:(.+)", [](std::smatch &m)->bool {
  return setup(net::endpoint<asio::local::stream_protocol>(m[1])) > 0;
});

static cli::option tcp("http:(.+):([0-9]+)", [](std::smatch &m)->bool {
  return setup(net::endpoint<asio::ip::tcp>(m[1], m[2])) > 0;
});

}
}

#endif

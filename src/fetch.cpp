/* HTTP client programme.
 *
 * Contains a very basic HTTP client, primarily to test the library against an
 * HTTP server running on a UNIX socket.
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
#define ASIO_DISABLE_THREADS
#define USE_DEFAULT_IO_MAIN
#include <cxxhttp/http.h>

using namespace cxxhttp;

namespace client {
template <class transport>
static std::size_t setup(net::endpoint<transport> lookup, std::string host,
                         std::string resource,
                         service &service = efgy::global<cxxhttp::service>()) {
  return lookup.with([&service, host, resource](
                         typename transport::endpoint &endpoint) -> bool {
    net::http::client<transport> *s =
        new net::http::client<transport>(endpoint, service);

    s->processor
        .query("GET", resource, {{"Host", host}, {"Keep-Alive", "none"}}, "")
        .then([](typename net::http::client<transport>::session &session)
                  -> bool {
          std::cout << session.content;
          return true;
        });

    return true;
  });
}

namespace cli {
using efgy::cli::option;

static option socket("-{0,2}http:unix:(.+):(.+)",
                     [](std::smatch &m) -> bool {
                       return setup(net::endpoint<asio::local::stream_protocol>(
                                        m[1]),
                                    m[1], m[2]) > 0;
                     },
                     "Fetch resource[2] via HTTP from unix socket[1].");

static option tcp("http://([^@:/]+)(:([0-9]+))?(/.*)",
                  [](std::smatch &m) -> bool {
                    const std::string port =
                        m[2] != "" ? std::string(m[3]) : std::string("80");
                    return setup(net::endpoint<asio::ip::tcp>(m[1], port), m[1],
                                 m[4]) > 0;
                  },
                  "Fetch the given HTTP URL.");
}
}

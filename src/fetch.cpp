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
#include <cxxhttp/http.h>

using namespace cxxhttp;

namespace client {
template <class sock>
static std::size_t setup(net::endpoint<sock> lookup, std::string host,
                         std::string resource,
                         io::service &service = io::service::common()) {
  return lookup.with([&service, host,
                      resource](typename sock::endpoint &endpoint) -> bool {
    net::http::client<sock> *s = new net::http::client<sock>(endpoint, service);

    s->processor
        .query("GET", resource, {{"Host", host}, {"Keep-Alive", "none"}}, "")
        .then([](typename net::http::client<sock>::session &session) -> bool {
          std::cout << session.content;
          return true;
        });

    return true;
  });
}

static efgy::cli::option socket(
    "-{0,2}http:unix:(.+):(.+)",
    [](std::smatch &m) -> bool {
      return setup(net::endpoint<asio::local::stream_protocol>(m[1]), m[1],
                   m[2]) > 0;
    },
    "Fetch resource[2] via HTTP from unix socket[1].");

static efgy::cli::option tcp(
    "http://([^@:/]+)(:([0-9]+))?(/.*)",
    [](std::smatch &m) -> bool {
      const std::string port =
          m[2] != "" ? std::string(m[3]) : std::string("80");
      return setup(net::endpoint<asio::ip::tcp>(m[1], port), m[1], m[4]) > 0;
    },
    "Fetch the given HTTP URL.");
}

/* Main function for the HTTP client demo.
 * @argc Process argument count.
 * @argv Process argument vector
 *
 * @return 0 when nothing bad happened, 1 otherwise.
 */
int main(int argc, char *argv[]) { return io::main(argc, argv); }

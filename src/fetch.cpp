/**\file
 * \ingroup example-programmes
 *
 * \copyright
 * This file is part of the libefgy project, which is released as open source
 * under the terms of an MIT/X11-style licence, described in the COPYING file.
 *
 * \see Project Documentation: https://ef.gy/documentation/libefgy
 * \see Project Source Code: https://github.com/ef-gy/libefgy
 * \see Licence Terms: https://github.com/ef-gy/libefgy/blob/master/COPYING
 */

#define ASIO_DISABLE_THREADS
#include <ef.gy/http.h>

using namespace efgy;

namespace client {
template <class sock>
static std::size_t setup(net::endpoint<sock> lookup, std::string host,
                         std::string resource,
                         io::service &service = io::service::common()) {
  return lookup.with([&service, host, resource](
                         typename sock::endpoint &endpoint) -> bool {
    net::http::client<sock> *s = new net::http::client<sock>(endpoint, service);

    s->processor.query("GET", resource,
                       "Host: " + host + "\r\nKeep-Alive: none\r\n", "")
        .then([](typename net::http::client<sock>::session &session) -> bool {
          std::cout << session.content;
          return true;
        });

    return true;
  });
}

static cli::option
    socket("-{0,2}http:unix:(.+):(.+)", [](std::smatch &m) -> bool {
      return setup(net::endpoint<asio::local::stream_protocol>(m[1]), m[1],
                   m[2]) > 0;
    }, "Fetch resource[2] via HTTP from unix socket[1].");

static cli::option
    tcp("http://([^@:/]+)(:([0-9]+))?(/.*)", [](std::smatch &m) -> bool {
      const std::string port =
          m[2] != "" ? std::string(m[3]) : std::string("80");
      return setup(net::endpoint<asio::ip::tcp>(m[1], port), m[1], m[4]) > 0;
    }, "Fetch the given HTTP URL.");
}

/**\brief Main function for the HTTP client demo.
 *
 * \param[in] argc Process argument count.
 * \param[in] argv Process argument vector
 *
 * \returns 0 when nothing bad happened, 1 otherwise.
 */
int main(int argc, char *argv[]) { return io::main(argc, argv); }

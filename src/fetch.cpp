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

#include <cxxhttp/http-client.h>

using cxxhttp::http::fetch;
using cxxhttp::net::endpoint;
using cxxhttp::transport::unix;
using cxxhttp::transport::tcp;
using efgy::cli::option;

namespace cli {
static option UNIX("-{0,2}http:unix:(.+):(.+)",
                   [](std::smatch &m) -> bool {
                     return fetch(endpoint<unix>(m[1]), m[1], m[2]);
                   },
                   "Fetch resource[2] via HTTP from unix socket[1].");

static option TCP("http://([^@:/]+)(:([0-9]+))?(/.*)",
                  [](std::smatch &m) -> bool {
                    const std::string port =
                        m[2] != "" ? std::string(m[3]) : std::string("80");
                    return fetch(endpoint<tcp>(m[1], port), m[1], m[4]);
                  },
                  "Fetch the given HTTP URL.");
}

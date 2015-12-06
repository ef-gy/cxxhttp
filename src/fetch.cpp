/**\file
 * \ingroup example-programmes
 *
 * \copyright
 * Copyright (c) 2015, ef.gy Project Members
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
 * \see Project Documentation: https://ef.gy/documentation/libefgy
 * \see Project Source Code: https://github.com/ef-gy/libefgy
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
    tcp("-{0,2}http:(.+):([0-9]+):(.+)", [](std::smatch &m) -> bool {
      return setup(net::endpoint<asio::ip::tcp>(m[1], m[2]), m[1], m[3]) > 0;
    }, "Fetch resource[3] via HTTP from host[1] and port[2].");
}

/**\brief Main function for the HTTP client demo.
 *
 * \param[in] argc Process argument count.
 * \param[in] argv Process argument vector
 *
 * \returns 0 when nothing bad happened, 1 otherwise.
 */
int main(int argc, char *argv[]) { return io::main(argc, argv); }

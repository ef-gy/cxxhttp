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

#include <unistd.h>

#include <cxxhttp/http-client.h>

using cxxhttp::http::call;
using cxxhttp::http::sessionData;
using cxxhttp::net::endpoint;
using cxxhttp::transport::unix;
using cxxhttp::transport::tcp;
using efgy::cli::option;

namespace cli {
static int output = STDOUT_FILENO;

static option outFD(
    "-{0,2}output-fd:([0-9]+)", [](std::smatch &m) -> bool {
                                  std::string fdn = m[1];
                                  output = std::stoi(fdn);
                                  return true;
                                },
    "send output to the given file descriptor; the descriptor must be open");

static option UNIX("-{0,2}http:unix:(.+):(.+)",
                   [](std::smatch &m) -> bool {
                     const std::string target = m[1];
                     const std::string path = m[2];
                     call<unix>(path, {{"Host", target}})
                         .success([](sessionData &sess) {
                           write(output, sess.content.c_str(),
                                 sess.content.size());
                         })
                         .failure([target, path](sessionData &sess) {
                           std::cerr << "Failed to retrieve URL: " << path
                                     << " from socket: " << target << "\n";
                         });
                     return true;
                   },
                   "fetch resource[2] via HTTP from unix socket[1]; error "
                   "output is on stderr");

static option TCP(
    "http://([^@:/]+)(:[0-9]+|:stdio)?(/.*)",
    [](std::smatch &m) -> bool {
      const std::string url = m[0];
      call<tcp>(url)
          .success([](sessionData &sess) {
            write(output, sess.content.c_str(), sess.content.size());
          })
          .failure([url](sessionData &sess) {
            std::cerr << "Failed to retrieve URL: " << url << "\n";
          });
      return true;
    },
    "fetch the given HTTP URL; talk on STDIO if the port is 'stdio'");
}

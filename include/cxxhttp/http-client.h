/* HTTP client wrapper helper code.
 *
 * Contains a very basic HTTP client, primarily to test the library against an
 * HTTP server running on a UNIX socket.
 *
 * There's an example programme using this in the src/fetch.cpp programme.
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
#if !defined(CXXHTTP_HTTP_CLIENT_H)
#define CXXHTTP_HTTP_CLIENT_H

#include <cxxhttp/http.h>
#include <iostream>

namespace cxxhttp {
namespace http {
/* Prepare and dispatch an HTTP client call.
 * @transport An ASIO transport type.
 * @uri What to get.
 * @header Additional headers for the request.
 * @content What to send as the request body.
 * @method The method to use when talking to the server.
 * @clients The global client set.
 * @service The ASIO IO service to use.
 *
 * This function prepares a client and a connection, in a way that makes it easy
 * to fetch a resource from some remote server.
 *
 * If the URL does not specify a host to connect to, the Host: header is used
 * instead. This allows connecting to UNIX sockets via HTTP, as the target
 * socket path would not otherwise fit in the authority field of a URL.
 *
 * This function actively ignores the scheme specified in the URL, if any.
 * That's because the function already says it'll use HTTP. On the downside,
 * that also means that HTTPS won't work with this function (because the library
 * does not currently support that). If this is a concern for you, use a socat
 * proxy or something.
 *
 * Example usage of this function:
 *
 *     const std::string url = "http://example.com/";
 *     call<tcp>(url)
 *         .success([](sessionData &sess) {
 *           std::cout << sess.content;
 *         })
 *         .failure([url](sessionData &sess) {
 *           std::cerr << "Failed to retrieve URL: " << url << "\n";
 *         });
 *
 * For an example with UNIX sockets, see src/fetch.cpp.
 *
 * @return An HTTP client reference, so you can set up success and failure
 * handlers like in the example.
 */
template <class transport>
static processor::client &call(
    const std::string &uri, headers header = {},
    const std::string &content = "", const std::string method = "GET",
    efgy::beacons<client<transport>> &clients =
        efgy::global<efgy::beacons<client<transport>>>(),
    service &service = efgy::global<cxxhttp::service>()) {
  cxxhttp::uri u = uri;
  std::regex rx("([^:]+)(:([0-9]+))?");
  std::smatch match;

  static processor::client failure;

  if (u.valid()) {
    std::string authority = u.authority();
    if (authority.empty()) {
      authority = header["Host"];
    }
    if (header["Host"].empty()) {
      header["Host"] = authority;
    }
    if (std::regex_match(authority, match, rx)) {
      const std::string host = match[1];
      const std::string port = match[3];
      const std::string serv = port.empty() ? "http" : port;
      net::endpoint<transport> endpoint(host, serv);
      try {
        for (net::endpointType<transport> e : endpoint) {
          try {
            auto &s = client<transport>::get(e, clients, service);

            s.processor.doFail = false;
            s.processor.query(method, u.path(), header, content);
            return s.processor;
          } catch (...) {
            // ignore setup and connection errors, which will fall through to
            // the specially crafted failure client.
          }
        }
      } catch (...) {
        // this will throw if the host to connect to can't be found, in which
        // case we want to fall back to returning the failure client.
      }
    }
  }

  if (!failure.doFail) {
    failure.doFail = true;
  }
  return failure;
}
}
}

#endif

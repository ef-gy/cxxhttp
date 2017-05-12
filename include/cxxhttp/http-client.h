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

namespace cxxhttp {
namespace http {
template <class transport>
static bool fetch(net::endpoint<transport> lookup, std::string host,
                  std::string resource,
                  std::set<client<transport> *> clients =
                      efgy::global<std::set<client<transport> *>>(),
                  service &service = efgy::global<cxxhttp::service>()) {
  return with(lookup, [&clients, &service, host, resource](
                          typename transport::endpoint &endpoint) -> bool {
    client<transport> *s = new client<transport>(endpoint, service);
    clients.insert(s);

    s->processor
        .query("GET", resource, {{"Host", host}, {"Keep-Alive", "none"}})
        .then([](typename client<transport>::session &session) -> bool {
          std::cout << session.content;
          return true;
        });

    return true;
  });
}
}
}

#endif

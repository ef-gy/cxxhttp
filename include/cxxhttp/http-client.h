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
/* Fetch a resource to a stream.
 * @transport Transport type of the endpoint.
 * @lookup The endpoint spec to look up.
 * @host What to send as the Host header in the request.
 * @resource Which resource to fetch.
 * @stream Where to write the resource to. Must not go out of scope.
 * @clients The list of clients to add this one to.
 * @service The IO service to use.
 *
 * Fetches a simple resource and writes the resulting information to the given
 * stream. Fetching and writing are performed asynchronously.
 *
 * If stream goes out of scope before it's used, that'd be bad.
 *
 * @return Reports whether setting up the asynchronous client succeeded.
 */
template <class transport>
static bool fetch(net::endpoint<transport> lookup, std::string host,
                  std::string resource, std::ostream &stream = std::cout,
                  std::set<client<transport> *> clients =
                      efgy::global<std::set<client<transport> *>>(),
                  service &service = efgy::global<cxxhttp::service>()) {
  for (net::endpointType<transport> endpoint : lookup) {
    client<transport> *s = new client<transport>(endpoint, clients, service);

    s->processor
        .query("GET", resource, {{"Host", host}, {"Keep-Alive", "none"}})
        .then([&stream](typename client<transport>::session &session) {
          stream << session.content;
        });

    return true;
  }

  return false;
}
}
}

#endif

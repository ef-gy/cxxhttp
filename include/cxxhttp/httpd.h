/* asio.hpp HTTP server.
 *
 * This file implements an actual HTTP server with a simple servlet capability
 * in top of http.h.
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
#if !defined(CXXHTTP_HTTPD_H)
#define CXXHTTP_HTTPD_H

#include <ef.gy/cli.h>
#include <ef.gy/global.h>

#include <cxxhttp/http.h>

namespace cxxhttp {
namespace httpd {
/* HTTP servlet container.
 * @transport What transport the servlet is for, e.g. transport::tcp.
 *
 * This contains all the data needed to set up a subprocessor for the default
 * server processor type. This consists of regexen to match against a requested
 * resource and the method invoked, as well as a handler for a succsseful match.
 *
 * For advanced usage, it is possible to provide data or content negotiation.
 */
template <class transport>
class servlet {
 public:
  /* Session type alias.
   *
   * This shortens a few lines below, as the type is surprisingly long to write
   * out.
   */
  using sessionType = typename http::server<transport>::session;

  /* Constructor.
   * @pResourcex Regex for applicable resources.
   * @pHandler Function specification to handle incoming requests that match.
   * @pMethodx Optional method regex; defaults to only allowing GET.
   * @pNegotiations Map of content negotiations to perform for this servlet.
   * @pDescription Optional API description string. URL recommended.
   * @pSet Where to register the servlet; defaults to the global set.
   *
   * Initialisation allows setting all the available options. The only reason
   * this isn't a POD type is that we also need to register with the
   * per-transport set of servlets.
   */
  servlet(const std::string &pResourcex,
          std::function<void(sessionType &, std::smatch &)> pHandler,
          const std::string &pMethodx = "GET",
          const http::headers pNegotiations = {},
          const std::string &pDescription = "no description available",
          std::set<servlet *> &pSet = efgy::global<std::set<servlet *>>())
      : resourcex(pResourcex),
        methodx(pMethodx),
        handler(pHandler),
        negotiations(pNegotiations),
        description(pDescription),
        root(pSet) {
    root.insert(this);
  }

  /* Destructor.
   *
   * Unregisters from the global set this servlet was registered to.
   */
  ~servlet(void) { root.erase(this); }

  /* Resource regex.
   *
   * A regex that is matched, in full, against any incoming requests. The
   * matches structure that results is passed into the handler function, so it
   * is recommended that interesting parts of the URL are captured using
   * parentheses to indicate subexpressions.
   */
  const std::string resourcex;

  /* Method regex.
   *
   * Similar to the resource regex, but for the method that is used by the
   * client. The matches struct here is not provided to the handler, but the
   * original method name is provided as part of the session object.
   */
  const std::string methodx;

  /* Content negotiation data.
   *
   * This is a map of the form `header: valid options`. Any header specified
   * will be automatically negotiated against what the client specifies during
   * the request, while allowing q-values on either side.
   *
   * If there's no matches between what both sides provide, then this is
   * considered a bad match, and a reply is sent to the client indicating that
   * content negotiation has failed.
   *
   * As an example, suppose this is set to:
   *
   *     {
   *       {"Accept": "text/plain, application/json;q=0.9"}
   *     }
   *
   * If the client sent no `Accept` header, the negotiated value would be
   * `text/plain`. If it sent the value `application/json` then this would pick
   * `application/json`. If it sent `foo/blargh` then that would be a bad match,
   * resulting in a client error unless a different servlet that is applicable
   * matches successfully.
   *
   * Also note that `Accept` in particular will cause an appropriate
   * `Content-Type` header with the correct negotiation to be sent back
   * automatically. This is configured in one of the constants in
   * `http-constants.h`. Also, each match will add the original header to the
   * `Vary` header that will also be added to the output automatically.
   */
  const http::headers negotiations;

  /* Handler function.
   *
   * Will be invoked if the resource and method match the provided regexen, and
   * content negotiation was successful.
   *
   * The return type is a boolean, and returning `true` indicates that the
   * request has been adequately processed. If this function returns `false`,
   * then the processor will continue trying to look up matching handlers and
   * assume no reply has been sent, yet.
   */
  const std::function<void(sessionType &, std::smatch &)> handler;

  /* Description of the servlet.
   *
   * Help texts may use this to provide more details on what a servlet does and
   * how to invoke it correctly. A URL with an API description si recommended.
   */
  const std::string description;

  /* Add servlet to processor.
   * @processor The HTTP processor type. Most likely http::processor::server.
   * @proc The HTTP processor instance to add to.
   *
   * Adds the servlet as a subprocessor to the given processor, allowing that
   * processor to use it.
   */
  template <class processor>
  void apply(processor &proc) const {
    proc.add(resourcex, handler, methodx, negotiations);
  }

 protected:
  /* The servlet set we're registered with.
   *
   * Used in the destructor to unregister the servlet.
   */
  std::set<servlet *> &root;
};

namespace cli {
using efgy::cli::option;

/* Set up an endpoint as an HTTP server.
 * @transport The transport type of the endpoint, e.g. transport::tcp.
 * @lookup The endpoint to bind to.
 * @servers The set of servers to add the newly set up instance to.
 * @service The I/O service to use, defaults to the global one.
 * @servlets The servlets to bind, defaults to the global set for the transport.
 *
 * This sets up an HTTP server on a new listening socket bound to whatever
 * `lookup` specifies. If that ends up being several different sockets, then
 * multiple servers are spawned.
 *
 * @return `true` if the setup was successful.
 */
template <class transport>
static bool setup(net::endpoint<transport> lookup,
                  std::set<http::server<transport> *> servers =
                      efgy::global<std::set<http::server<transport> *>>(),
                  service &service = efgy::global<cxxhttp::service>(),
                  std::set<servlet<transport> *> &servlets =
                      efgy::global<std::set<servlet<transport> *>>()) {
  bool rv = false;

  for (net::endpointType<transport> endpoint : lookup) {
    http::server<transport> *s =
        new http::server<transport>(endpoint, servers, service);

    for (const auto &sv : servlets) {
      sv->apply(s->processor);
    }

    rv = rv || true;
  }

  return rv;
}

/* Set up an HTTP server on a TCP socket.
 * @match The matches from the TCP regex.
 *
 * This uses setup() to create a server on the interface specified with match[1]
 * and the port specified with match[2].
 * The server will have the default HTTP processor, with all registered TCP
 * servlets applied.
 *
 * @return 'true' if the setup was successful.
 */
static inline bool setupTCP(std::smatch &match) {
  return setup(net::endpoint<transport::tcp>(match[1], match[2]));
}

/* Set up an HTTP server on a UNIX socket.
 * @match The matches from the UNIX regex.
 *
 * This uses setup() to create a server on the socket file specified with
 * match[1]. The server will have the default HTTP processor, with all
 * registered UNIX servlets applied.
 *
 * @return 'true' if the setup was successful.
 */
static inline bool setupUNIX(std::smatch &match) {
  return setup(net::endpoint<transport::unix>(match[1]));
}

/* TCP HTTP server CLI option.
 *
 * The format is `http:<interface-address>:<port>`. The server that is set up
 * will have all available servlets registered.
 */
static option TCP(
    "-{0,2}http:(.+):([0-9]+)", setupTCP,
    "listen for HTTP connections on the given host[1] and port[2]");

/* UNIX socket HTTP server CLI option.
 *
 * The format is `http:unix:<socket-file>`. The server that is set up will have
 * all available servlets registered.
 */
static option UNIX("-{0,2}http:unix:(.+)", setupUNIX,
                   "listen for HTTP connections on the given unix socket[1]");
}

namespace usage {
using efgy::cli::hint;

/* Create usage hint.
 * @transport Used to look up the correct set of servlets, e.g. transpor::tcp.
 *
 * This creates a brief textual summary of the available servlets, with the
 * regular expressions that are used to match them against incoming queries.
 *
 * @return A string with a summary of the available servlets.
 */
template <typename transport>
static std::string describe(void) {
  std::string rv;
  const auto &servlets = efgy::global<std::set<servlet<transport> *>>();
  for (const auto &servlet : servlets) {
    rv += " * " + servlet->methodx + " " + servlet->resourcex + "\n   " +
          servlet->description + "\n";
  }
  return rv;
}

/* TCP servlet hints.
 *
 * Enables a description of all available TCP servlets for the `--help` flag.
 */
static hint TCP("HTTP Endpoints (TCP)", describe<transport::tcp>);

/* UNIX socket servlet hints.
 *
 * Enables a description of all available UNIX servlets for the `--help` flag.
 */
static hint UNIX("HTTP Endpoints (UNIX)", describe<transport::unix>);
}
}
}

#endif

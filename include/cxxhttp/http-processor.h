/* HTTP processors.
 *
 * For higher level HTTP protocol handling.
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
#if !defined(CXXHTTP_HTTP_PROCESSOR_H)
#define CXXHTTP_HTTP_PROCESSOR_H

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <regex>

#include <cxxhttp/header.h>
#include <cxxhttp/network.h>
#include <cxxhttp/version.h>

#include <cxxhttp/http-constants.h>

namespace cxxhttp {
namespace http {
template <typename base, typename requestProcessor>
class session;

static const headers sendNegotiatedAs{
    {"Accept", "Content-Type"},
};

static const headers defaultServerHeaders{
    {"Server", identifier},
};

static const headers defaultClientHeaders{
    {"User-Agent", identifier},
};

/* HTTP processors
 *
 * This namespace is reserved for HTTP "processors", which contain the logic to
 * actually handle an HTTP request after it has been parsed.
 */
namespace processor {
/* Subprocessor type.
 * @session The session type for the subprocessor.
 *
 * Used in the basic server processor to dispatch requests.
 */
template <class session>
struct subprocessor {
  std::regex resource;
  std::regex method;
  headers negotiations;
  std::function<bool(session &, std::smatch &)> handler;
};

/* Base processor
 * @transport The socket class for the request, e.g. transport::tcp
 *
 * This is the default processor, which fans out incoming requests by means of a
 * set of regular expressions. If a regex matches, a corresponding function is
 * called, which gets the session and the regex match results.
 *
 * If no regex should match, a 404 response is generated.
 *
 * If you need to keep track of custom, per-server data, then the best way to do
 * so would probably involve extending this object and adding the data you need.
 * A reference to the processor is available via the the session object, and it
 * is kept around for as long as the server object is.
 */
template <class transport>
class base {
 public:
  /* Session type
   *
   * This is the session type that the processor is intended for. This typedef
   * is mostly here for convenience.
   */
  typedef session<transport, base> session;

  /* Handle request
   * @sess The session object where the request was made.
   *
   * This is the generic inbound request handler. Whenever a new request needs
   * to be handled, this will go through the registered list of regexen, and for
   * all that match it will call the registered function, until one of them
   * returns true.
   */
  void operator()(session &sess) const {
    std::set<std::string> methods{};
    bool trigger405 = false;
    bool trigger406 = false;
    bool methodSupported = false;

    for (auto &proc : subprocessor) {
      const auto &subprocessor = proc.second;
      std::smatch matches;

      bool resourceMatch =
          std::regex_match(sess.resource, matches, subprocessor.resource);
      bool methodMatch = std::regex_match(sess.method, subprocessor.method);

      methodSupported = methodSupported || methodMatch;

      if (resourceMatch) {
        if (methodMatch) {
          bool badNegotiation = false;
          // reset, and perform, header value negotiation based on the
          // subprocessor's specs and the client data.
          sess.negotiated = {};
          sess.outbound = defaultServerHeaders;
          for (const auto &n : subprocessor.negotiations) {
            const std::string cv =
                sess.header.count(n.first) > 0 ? sess.header[n.first] : "";
            const std::string v = negotiate(cv, n.second);

            // modify the Vary value to indicate we used this header.
            append(sess.outbound, "Vary", n.first);

            sess.negotiated[n.first] = v;

            const auto it = sendNegotiatedAs.find(n.first);
            if (it != sendNegotiatedAs.end()) {
              sess.outbound[it->second] = v;
            }

            badNegotiation = badNegotiation || v.empty();
          }

          if (badNegotiation) {
            trigger406 = true;
          } else if (subprocessor.handler(sess, matches)) {
            return;
          }
          methods.insert(sess.method);
        } else
          for (const auto &m : http::method) {
            if (std::regex_match(m, subprocessor.method)) {
              methods.insert(m);
            }
          }
      }
    }

    if (!methodSupported) {
      sess.reply(501, "Sorry, this method is not supported by this server.");
      return;
    }

    // To trigger the 405 response, we want the list of allowed methods to
    // be non-empty, but we also don't want the list to only consist of methods
    // which are expected to be valid for every resource in the first place.
    // (Though this would technically be correct as well, it would be unexpected
    // of an HTTP server since everyone else seems to be ignoring the OPTIONS
    // method and people don't commonly allow TRACE.)
    for (const auto &m : methods) {
      if (non405method.find(m) == non405method.end()) {
        trigger405 = true;
        break;
      }
    }

    if (trigger406) {
      sess.reply(406,
                 "Sorry, negotiating the resource's representation failed.");
      return;
    }

    if (trigger405 && (methods.size() > 0)) {
      headers h{};
      for (const auto &m : methods) {
        append(h, "Allow", m);
      }

      sess.reply(405, h,
                 "Sorry, this resource is not available via this method.");
      return;
    }

    sess.reply(404, "Sorry, this resource was not found.");
  }

  /* Add handler
   * @rx The regex that should trigger a given handler.
   * @handler The function to call.
   * @methodx Regex for allowed methods.
   * @negotiations q-value lists for server-side content negotiation.
   *
   * This function adds a handler for a specific regex. Currently this is stored
   * in a map, so the order is probably unpredictable - but also probably just
   * alphabetic.
   */
  void add(const std::string &rx,
           std::function<bool(session &, std::smatch &)> handler,
           const std::string &methodx = "GET",
           const headers &negotiations = {}) {
    subprocessor[rx] = {std::regex(rx), std::regex(methodx), negotiations,
                        handler};
  }

  /* Begin handling requests
   * @sess The session that's just been established.
   *
   * Called by a specific session object to indicate that a session and
   * connection have now been established.
   *
   * In the HTTP server case, we begin by reading.
   */
  void start(session &sess) {
    //  sess.read();
  }

 protected:
  /* The subprocessor type.
   *
   * Assembled using the local session type.
   */
  using processor = subprocessor<session>;

  /* Map of request handlers
   *
   * This is the map that holds the request handlers. It maps regex strings to
   * handler functions, which is fairly straightforward.
   */
  std::map<std::string, processor> subprocessor;
};

/* Client request data.
 *
 * This is all the data needed to do a client request, assuming you're already
 * connected, anyway.
 */
struct request {
  std::string method;
  std::string resource;
  headers header;
  std::string body;
};

/* Basic HTTP client processor.
 * @transport The transport type, e.g. transport::tcp.
 *
 * Like the basic server processor, but handles client requests.
 */
template <class transport>
class client {
 public:
  /* Session type
   *
   * This is the session type that the processor is intended for. This typedef
   * is mostly here for convenience.
   */
  using session = session<transport, client>;

  bool operator()(session &sess) const {
    if (onSuccess) {
      return onSuccess(sess);
    }

    return true;
  }

  void start(session &sess) {
    auto req = requests.front();
    requests.pop_front();

    sess.request(req.method, req.resource, req.header, req.body);
  }

  client &query(const std::string &method, const std::string &resource,
                const headers &header, const std::string &body) {
    requests.push_back(request{method, resource, header, body});
    return *this;
  }

  client &then(std::function<bool(session &)> callback) {
    onSuccess = callback;
    return *this;
  }

 protected:
  std::deque<request> requests;
  std::function<bool(session &)> onSuccess;
};
}
}
}

#endif

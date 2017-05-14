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

#include <cxxhttp/negotiate.h>
#include <cxxhttp/network.h>
#include <cxxhttp/version.h>

#include <cxxhttp/http-constants.h>
#include <cxxhttp/http-session.h>

namespace cxxhttp {
namespace http {
template <typename transport, typename requestProcessor>
class session;

/* HTTP header negotiation map.
 *
 * Maps input header names to their equivalent outbound version.
 */
static const headers sendNegotiatedAs{
    {"Accept", "Content-Type"},
};

/* Default servers headers.
 *
 * These headers are sent by default with every server reply, unless overriden.
 */
static const headers defaultServerHeaders{
    {"Server", identifier},
};

/* Default client headers.
 *
 * These headers are sent by default with every client request, unless
 * overriden.
 */
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
struct handler {
  /* Compiled resource regex.
   *
   * Matched against the resource that the client is requesting.
   */
  std::regex resource;

  /* Compiled method regex.
   *
   * Matched against the method that the client used.
   */
  std::regex method;

  /* List of applicable server-side content negotiations.
   *
   * If present, these automatically get resolved against what the client sends
   * and the outbound headers are updated accordingly. Will also generate a Vary
   * header to go along with it.
   */
  headers negotiations;

  /* Handling function.
   *
   * Called if the resource and method are a match and the negotiations, if
   * there were any, were successful.
   *
   * After this handler is run, the processor examines the session to see if you
   * sent out a reply. If so, it will stop future processing.
   */
  std::function<void(session &, std::smatch &)> handler;
};

/* Transport-agnostic parts of the server processor.
 *
 * These are transport-agnostic bits of data and functions that are used by the
 * server processor, so as to deduplicate some of the functions that would
 * otherwise be expanded per target transport type.
 * target transport type.
 */
class serverData {
 public:
  /* Maximum request content size
   *
   * The maximum number of octets supported for a request body. Requests larger
   * than this are cancelled with an error.
   */
  std::size_t maxContentLength = (1024 * 1024 * 12);

  /* Negotiate headers for request.
   * @sess Basic session data, which is modified and initialised in-place.
   * @negotiations The set of negotiations to perform, from the subprocessor.
   *
   * Uses the global header negotiation facilities to set actual inbound and
   * outbound headers based on what the input request looks like.
   *
   * @return Whether or not negotiations were successful.
   */
  bool negotiateHeaders(sessionData &sess, const headers &negotiations) const {
    bool badNegotiation = false;
    // reset, and perform, header value negotiation based on the
    // subprocessor's specs and the client data.
    sess.negotiated = {};
    sess.outbound = {defaultServerHeaders};
    for (const auto &n : negotiations) {
      const std::string cv =
          sess.header.count(n.first) > 0 ? sess.header[n.first] : "";
      const std::string v = negotiate(cv, n.second);

      // modify the Vary value to indicate we used this header.
      sess.outbound.append("Vary", n.first);

      sess.negotiated[n.first] = v;

      const auto it = sendNegotiatedAs.find(n.first);
      if (it != sendNegotiatedAs.end()) {
        sess.outbound.header[it->second] = v;
      }

      badNegotiation = badNegotiation || v.empty();
    }

    return !badNegotiation;
  }

  /* Decide whether to trigger a 405.
   * @methods The methods we've seen as allowed during processing.
   *
   * To trigger the 405 response, we want the list of allowed methods to
   * be non-empty, but we also don't want the list to only consist of methods
   * which are expected to be valid for every resource in the first place.
   * (Though this would technically be correct as well, it would be unexpected
   * of an HTTP server since everyone else seems to be ignoring the OPTIONS
   * method and people don't commonly allow TRACE.)
   *
   * @return Whether or not a 405 is more appropriate than a 404.
   */
  bool trigger405(const std::set<std::string> &methods) const {
    for (const auto &m : methods) {
      if (non405method.find(m) == non405method.end()) {
        return true;
      }
    }

    return false;
  }
};

/* Basis server processor
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
class server : public serverData {
 public:
  /* Session type
   *
   * This is the session type that the processor is intended for. This typedef
   * is mostly here for convenience, but it also helps the connection type to
   * figure out what session this processor needs.
   */
  using session = http::session<transport, server>;

  /* Handle request
   * @sess The session object where the request was made.
   *
   * This is the generic inbound request handler. Whenever a new request needs
   * to be handled, this will go through the registered list of regexen, and for
   * all that match it will call the registered function, until one of them
   * returns true.
   */
  void handle(session &sess) const {
    std::set<std::string> methods;
    bool trigger406 = false;
    bool methodSupported = false;

    const std::string resource = sess.inboundRequest.resource.path();
    const std::string method = sess.inboundRequest.method;

    for (auto &proc : subprocessor) {
      const auto &subprocessor = proc.second;
      std::smatch matches;

      bool resourceMatch =
          std::regex_match(resource, matches, subprocessor.resource);
      bool methodMatch = std::regex_match(method, subprocessor.method);

      methodSupported = methodSupported || methodMatch;

      if (resourceMatch) {
        if (methodMatch) {
          bool badNegotiation =
              !negotiateHeaders(sess, subprocessor.negotiations);

          if (badNegotiation) {
            trigger406 = true;
          } else {
            const std::size_t q = sess.queries();
            subprocessor.handler(sess, matches);

            if (sess.queries() > q) {
              // we've sent something back to the client, so no need to process
              // any further.
              return;
            }
          }

          methods.insert(method);
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

    if (trigger406) {
      sess.reply(406,
                 "Sorry, negotiating the resource's representation failed.");
      return;
    }

    if (trigger405(methods) && (methods.size() > 0)) {
      parser<headers> p{};
      for (const auto &m : methods) {
        p.append("Allow", m);
      }

      sess.reply(405, p.header,
                 "Sorry, this resource is not available via this method.");
      return;
    }

    sess.reply(404, "Sorry, this resource was not found.");
  }

  /* Decide whether to expect content or not.
   * @sess The session that just finished parsing headers.
   *
   * This function implements the logic necessary for determining whether there
   * will be content to parse or not.
   *
   * @return The parser state to switch to.
   */
  enum status afterHeaders(session &sess) const {
    const auto &cli = sess.header.find("Content-Length");
    const auto &exp = sess.header.find("Expect");

    if (exp != sess.header.end()) {
      if (exp->second == "100-continue") {
        sess.reply(100, "");
      } else {
        sess.reply(417, "Expectation Failed");
        return stError;
      }
    }

    if (cli != sess.header.end()) {
      try {
        sess.contentLength = std::stoi(cli->second);
      } catch (...) {
        sess.contentLength = 0;
      }

      if (sess.contentLength > maxContentLength) {
        sess.reply(413, "Request Entity Too Large");
        return stError;
      }
    } else {
      sess.contentLength = 0;
    }

    return stContent;
  }

  /* Decide what to do after handling a request.
   * @sess The session, after a request was handled.
   *
   * Decides what to do with an open connection to a server after the request
   * has been processed.
   *
   * @return The parser state to switch to.
   */
  enum status afterProcessing(session &sess) const {
    sess.status = stRequest;
    sess.read();
    return stRequest;
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
           std::function<void(session &, std::smatch &)> handler,
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
  void start(session &sess) const { sess.read(); }

 protected:
  /* The subprocessor type.
   *
   * Assembled using the local session type.
   */
  using processor = handler<session>;

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
  /* The request method.
   *
   * I.e. GET, POST, OPTIONS, etc.
   */
  std::string method;

  /* The requested resource.
   *
   * Should be an absolute or relative URI, or "*".
   */
  std::string resource;

  /* Additional request headers.
   *
   * Will be sent along with any default headers that the library sends.
   */
  headers header;

  /* Request body.
   *
   * Any actual request body to send. Note that the library is not really going
   * to want to send an empty (0-byte) request body and will instead just drop
   * it.
   */
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
  using session = http::session<transport, client>;

  /* Process result of request.
   * @sess The session with the fully processed request.
   *
   * Called once a request has been fully processed. Will trigger further
   * processing if anything is pending.
   */
  void handle(session &sess) {
    if (onSuccess) {
      onSuccess(sess);
    }

    start(sess);
  }

  /* Decide whether to expect content or not.
   * @sess The session that just finished parsing headers.
   *
   * This function implements the logic necessary for determining whether there
   * will be content to parse or not.
   *
   * @return The parser state to switch to.
   */
  enum status afterHeaders(session &sess) const {
    const auto &cli = sess.header.find("Content-Length");

    if (cli != sess.header.end()) {
      try {
        sess.contentLength = std::stoi(cli->second);
      } catch (...) {
        sess.contentLength = 0;
      }
    } else {
      sess.contentLength = 0;
    }

    return stContent;
  }

  /* Decide what to do after handling a request.
   * @sess The session, after a request was handled.
   *
   * Decides what to do with an open connection to a server after the request
   * has been processed.
   *
   * @return The parser state to switch to.
   */
  enum status afterProcessing(session &sess) const { return stShutdown; }

  /* Start processing requests.
   * @sess The session to process requests on.
   *
   * Pops a new request off the list of pending requests, and processes it, if
   * there is something to process.
   */
  void start(session &sess) {
    if (requests.size() == 0) {
      // nothing to do.
      return;
    }

    auto req = requests.front();

    requests.pop_front();

    sess.request(req.method, req.resource, req.header, req.body);
    sess.read();
  }

  /* Queue up things to do on this connection.
   * @method The method for the request. Use GET if you're not sure.
   * @resource The resource to query from the server.
   * @header Any additional headers to send.
   * @body The body of the request to send. Optional.
   *
   * Enqueues a new query to run on this connection, as appropriate.
   *
   * @return A reference to this object, for easier pipelining of requests.
   */
  client &query(const std::string &method, const std::string &resource,
                const headers &header, const std::string &body = "") {
    requests.push_back(request{method, resource, header, body});
    return *this;
  }

  /* Set function to call upon completion.
   * @callback The post-completion callback.
   *
   * The naming here is vaguely in line with the naming scheme used in recent
   * JavaScript libraries.
   *
   * @return A reference to the object's instance, to allow for chaining of
   * function calls.
   */
  client &then(std::function<void(session &)> callback) {
    onSuccess = callback;
    return *this;
  }

 protected:
  /* Request pool.
   *
   * Will be processed in sequence until either the connection is closed or the
   * list is empty. This allows for pipelining on the client side.
   */
  std::deque<request> requests;

  /* Success callback.
   *
   * Called when a server has returned something to one of our queries.
   */
  std::function<void(session &)> onSuccess;
};
}
}
}

#endif

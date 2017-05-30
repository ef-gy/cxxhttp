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

#include <cxxhttp/negotiate.h>
#include <cxxhttp/network.h>

#include <cxxhttp/http-constants.h>
#include <cxxhttp/http-error.h>
#include <cxxhttp/http-servlet.h>
#include <cxxhttp/http-session.h>

namespace cxxhttp {
namespace http {
template <typename transport, typename requestProcessor>
class session;
/* Default servers headers.
 *
 * These headers are sent by default with every server reply, unless overriden.
 */
static const headers defaultServerHeaders{
    {"Server", identifier},
};

/* HTTP processors
 *
 * This namespace is reserved for HTTP "processors", which contain the logic to
 * actually handle an HTTP request after it has been parsed.
 */
namespace processor {
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

  /* Bound servlets.
   *
   * This is the list of server-side request handlers we'll be using.
   * Deliberately not a reference, so that different servers can have different
   * servlets.
   */
  efgy::beacons<http::servlet> servlets;

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
    bool badNegotiation = false;
    bool methodSupported = false;

    const std::string resource = sess.inboundRequest.resource.path();
    const std::string method = sess.inboundRequest.method;

    for (const auto &servlet : servlets) {
      std::smatch matches;

      bool resourceMatch =
          std::regex_match(resource, matches, servlet->resource);
      bool methodMatch = std::regex_match(method, servlet->method);

      methodSupported = methodSupported || methodMatch;

      if (resourceMatch) {
        if (methodMatch) {
          sess.outbound = {defaultServerHeaders};
          badNegotiation =
              badNegotiation || !sess.negotiate(servlet->negotiations);

          if (!badNegotiation) {
            const std::size_t q = sess.queries();
            servlet->handler(sess, matches);

            if (sess.queries() > q) {
              // we've sent something back to the client, so no need to process
              // any further.
              return;
            }
          }

          methods.insert(method);
        } else
          for (const auto &m : http::method) {
            if (std::regex_match(m, servlet->method)) {
              methods.insert(m);
            }
          }
      }
    }

    error<session> e(sess);

    if (!methodSupported) {
      e.reply(501);
    } else if (badNegotiation) {
      e.reply(406);
    } else if (sess.trigger405(methods)) {
      e.allow = methods;
      e.reply(405);
    } else {
      e.reply(404);
    }
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
    const auto &cli = sess.inbound.header.find("Content-Length");
    const auto &exp = sess.inbound.header.find("Expect");

    if (exp != sess.inbound.header.end()) {
      if (exp->second == "100-continue") {
        sess.reply(100, "");
      } else {
        error<session>(sess).reply(417);
        return stError;
      }
    }

    if (cli != sess.inbound.header.end()) {
      try {
        sess.contentLength = std::stoi(cli->second);
      } catch (...) {
        sess.contentLength = 0;
      }

      if (sess.contentLength > maxContentLength) {
        error<session>(sess).reply(413);
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
    sess.readLine();
    return stRequest;
  }

  /* Begin handling requests
   * @sess The session that's just been established.
   *
   * Called by a specific session object to indicate that a session and
   * connection have now been established.
   *
   * In the HTTP server case, we begin by reading.
   */
  void start(session &sess) const { sess.status = afterProcessing(sess); }
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
    const auto &cli = sess.inbound.header.find("Content-Length");

    if (cli != sess.inbound.header.end()) {
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
    if (requests.size() > 0) {
      auto req = requests.front();

      requests.pop_front();

      sess.request(req.method, req.resource, req.header, req.body);
      sess.send();
      sess.readLine();
    }
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

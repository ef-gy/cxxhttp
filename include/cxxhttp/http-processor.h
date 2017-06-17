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
#include <functional>
#include <list>

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
/* Basis server processor
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
class server {
 public:
  /* Maximum request content size
   *
   * The maximum number of octets supported for a request body. Requests larger
   * than this are cancelled with an error.
   */
  std::size_t maxContentLength = (1024 * 1024 * 12);

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
   * returns and has sent a response.
   */
  void handle(sessionData &sess) const {
    std::set<std::string> methods;
    bool badNegotiation = false;
    bool methodSupported = false;

    const std::string resource = sess.inboundRequest.resource.path();
    const std::string resourceAndQuery = sess.inboundRequest.resource.path() +
                                         "?" +
                                         sess.inboundRequest.resource.query();
    const std::string method = sess.inboundRequest.method;
    sess.isHEAD = method == "HEAD";

    for (const auto &servlet : servlets) {
      std::smatch matches;

      bool resourceMatch =
          std::regex_match(resource, matches, servlet->resource) ||
          std::regex_match(resourceAndQuery, matches, servlet->resource);
      bool methodMatch = std::regex_match(method, servlet->method);

      if (!methodMatch && sess.isHEAD) {
        methodMatch = std::regex_match("GET", servlet->method);
      }

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

    error e(sess);

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
  enum status afterHeaders(sessionData &sess) const {
    const auto &cli = sess.inbound.header.find("Content-Length");
    const auto &exp = sess.inbound.header.find("Expect");

    if (exp != sess.inbound.header.end()) {
      if (exp->second == "100-continue") {
        sess.reply(100, "");
      } else {
        error(sess).reply(417);
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
        error(sess).reply(413);
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
  enum status afterProcessing(sessionData &sess) const { return stRequest; }

  /* Begin handling requests
   * @sess The session that's just been established.
   *
   * Called by a specific session object to indicate that a session and
   * connection have now been established.
   *
   * In the HTTP server case, we begin by reading.
   */
  void start(sessionData &sess) const { sess.status = afterProcessing(sess); }

  /* Whether to listen for a connection.
   *
   * 'true' for servers, i.e. if we want to listen for inbound queries.
   *
   * @return Always true, this is a server.
   */
  static bool listen(void) { return true; }

  /* Do stuff upon recycling a session.
   * @sess The session being recycled.
   *
   * Called right before the session is recycled, with a reference to the plain
   * version of it.
   *
   * There's nothing to do here for the server, so we just don't do anything.
   */
  void recycle(sessionData &sess) {}
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

/* Basic client processor.
 *
 * This provides a basic client processor, which retrieves data and, on success,
 * will call a user-specified callback, with the full session. This allows for
 * further processing by a client.
 */
class client {
 public:
  /* Auto-trigger failure response.
   *
   * When true, trying to set a handler with failure() or then() will
   * automatically trigger the failure response. This is to allow failure
   * callbacks when name resolution for clients fails entirely.
   */
  bool doFail = false;

  /* Was the last reply informational?
   *
   * We ignore these replies, so just resume as you normally would have.
   */
  bool gotInformationalResponse = false;

  /* Process result of request.
   * @sess The session with the fully processed request.
   *
   * Called once a request has been fully processed. Will dispatch to the
   * callback that the user gave us.
   */
  void handle(sessionData &sess) {
    if (sess.inboundStatus.valid()) {
      if (sess.inboundStatus.code >= 100 && sess.inboundStatus.code < 200) {
        gotInformationalResponse = true;
        return;
      }
      if (sess.inboundStatus.code >= 200 && sess.inboundStatus.code < 400) {
        if (onSuccess) {
          onSuccess(sess);
        }
        return;
      }
    }
    if (onFailure) {
      onFailure(sess);
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
  enum status afterHeaders(sessionData &sess) const {
    if (sess.isHEAD) {
      // if this is a HEAD request, ignore any Content-Length headers and assume
      // the response size will be zero octets long.
      // We have this because HEAD is allowed (but not required) to produce a
      // Content-Length header, which if present would have to be correct for
      // what GET would return.
      sess.contentLength = 0;
    } else {
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
  enum status afterProcessing(sessionData &sess) {
    if (gotInformationalResponse) {
      gotInformationalResponse = false;
      return stStatus;
    } else if (requests.size() > 0) {
      auto req = requests.front();

      requests.pop_front();

      sess.request(req.method, req.resource, req.header, req.body);
      return stStatus;
    } else {
      return stShutdown;
    }
  }

  /* Start processing requests.
   * @sess The session to process requests on.
   *
   * Pops a new request off the list of pending requests, and processes it, if
   * there is something to process.
   */
  void start(sessionData &sess) { sess.status = afterProcessing(sess); }

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
  client &then(std::function<void(sessionData &)> callback) {
    return success(callback), failure(callback);
  }

  /* Set function to call upon success.
   * @callback The post-completion callback.
   *
   * The naming here is vaguely in line with the naming scheme used in recent
   * JavaScript libraries.
   *
   * @return A reference to the object's instance, to allow for chaining of
   * function calls.
   */
  client &success(std::function<void(sessionData &)> callback) {
    onSuccess = callback;
    return *this;
  }

  /* Set function to call upon failure.
   * @callback The post-completion callback.
   *
   * The naming here is vaguely in line with the naming scheme used in recent
   * JavaScript libraries.
   *
   * @return A reference to the object's instance, to allow for chaining of
   * function calls.
   */
  client &failure(std::function<void(sessionData &)> callback) {
    onFailure = callback;
    if (doFail && onFailure) {
      static sessionData none;
      onFailure(none);
    }
    return *this;
  }

  /* Whether to listen for a connection.
   *
   * 'true' for servers, i.e. if we want to listen for inbound queries.
   *
   * @return Always false, this is a client.
   */
  static bool listen(void) { return false; }

  /* Do stuff upon recycling a session.
   * @sess The session being recycled.
   *
   * Called right before the session is recycled, with a reference to the plain
   * version of it.
   */
  void recycle(sessionData &sess) {
    // do something here if we still had queries to send.
    requests.clear();
  }

 protected:
  /* Request pool.
   *
   * Will be processed in sequence until either the connection is closed or the
   * list is empty. This allows for pipelining on the client side.
   */
  std::list<request> requests;

  /* Success callback.
   *
   * Called when a server has returned something to one of our queries.
   */
  std::function<void(sessionData &)> onSuccess;

  /* Failure callback.
   *
   * Called when being recycled without valid content, or whenever a non-good
   * return status is encountered.
   */
  std::function<void(sessionData &)> onFailure;
};
}
}
}

#endif

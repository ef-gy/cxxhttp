/**\file
 * \brief asio.hpp HTTP Server
 *
 * An asynchronous HTTP server implementation using asio.hpp and std::regex.
 *
 * \copyright
 * This file is part of the cxxhttp project, which is released as open source
 * under the terms of an MIT/X11-style licence, described in the COPYING file.
 *
 * \see Project Documentation: https://ef.gy/documentation/cxxhttp
 * \see Project Source Code: https://github.com/ef-gy/cxxhttp
 * \see Licence Terms: https://github.com/ef-gy/cxxhttp/blob/master/COPYING
 */

#if !defined(CXXHTTP_HTTP_H)
#define CXXHTTP_HTTP_H

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <regex>
#include <system_error>

#include <cxxhttp/client.h>
#include <cxxhttp/server.h>
#include <cxxhttp/version.h>

namespace cxxhttp {
namespace net {
/**\brief HTTP handling
 *
 * Contains an HTTP server and templates for session management and
 * processing by user code.
 */
namespace http {

/**\brief Known HTTP Status codes.
 *
 * \see https://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html#sec10
 * \see https://tools.ietf.org/html/rfc7725
 */
template <typename base, typename requestProcessor>
class session;
static const std::map<unsigned, const char *> status{
    // 1xx - Informational
    {100, "Continue"},
    {101, "Switching Protocols"},
    // 2xx - Successful
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    // 3xx - Redirection
    {300, "Multiple Choices"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {307, "Temporary Redirect"},
    // 4xx - Client Error
    {400, "Client Error"},
    {401, "Unauthorised"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Request Entity Too Large"},
    {414, "Request-URI Too Long"},
    {415, "Unsupported Media Type"},
    {416, "Requested Range Not Satisfiable"},
    {417, "Expectation Failed"},
    {451, "Unavailable For Legal Reasons"},
    // 5xx - Server Error
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"},
};

static const std::set<std::string> method{
    "OPTIONS", "GET", "HEAD", "POST", "PUT", "DELETE", "TRACE", "CONNECT",
};

static const std::set<std::string> non405method{
    "OPTIONS", "TRACE",
};

namespace comparator {
/**\brief Case-insensitive comparison functor
 *
 * A simple functor used by the attribute map to compare strings without
 * caring for the letter case.
 */
class headerNameLT
    : private std::binary_function<std::string, std::string, bool> {
 public:
  /**\brief Case-insensitive string comparison
   *
   * Compares two strings case-insensitively.
   *
   * \param[in] a The first of the two strings to compare.
   * \param[in] b The second of the two strings to compare.
   *
   * \returns 'true' if the first string is "less than" the second.
   */
  bool operator()(const std::string &a, const std::string &b) const {
    return std::lexicographical_compare(
        a.begin(), a.end(), b.begin(), b.end(),
        [](const unsigned char &c1, const unsigned char &c2) -> bool {
          return tolower(c1) < tolower(c2);
        });
  }
};
}

using headers = std::map<std::string, std::string, comparator::headerNameLT>;

/**\brief Flatten a header map.
 *
 * This function takes a header map and converts it into the form used in the
 * HTTP protocol. This form is, essentially, "Key: Value<CR><LN>".
 *
 * \param[in] header The header map to turn into a string.
 *
 * \returns A string, with all of the elements in the header parameter.
 */
static inline std::string flatten(const headers &header) {
  std::string r = "";
  for (const auto &h : header) {
    r += h.first + ": " + h.second + "\r\n";
  }
  return r;
}

/**\brief HTTP processors
 *
 * This namespace is reserved for HTTP "processors", which contain the logic to
 * actually handle an HTTP request after it has been parsed.
 */
namespace processor {
/**\brief Base processor
 *
 * This is the default processor, which fans out incoming requests by means of a
 * set of regular expressions. If a regex matches, a corresponding function is
 * called, which gets the session and the regex match results.
 *
 * If no regex should match, a 404 response is generated.
 *
 * \note If you need to keep track of custom, per-server data, then the best way
 *       to do so would probably involve extending this object and adding the
 *       data you need. A reference to the processor is available via the
 *       the session object, and it is kept around for as long as the server
 *       object is.
 *
 * \tparam sock The socket class for the request, e.g. asio::ip::tcp
 */
template <class sock>
class base {
 public:
  /**\brief Session type
   *
   * This is the session type that the processor is intended for. This typedef
   * is mostly here for convenience.
   */
  typedef session<sock, base<sock>> session;

  /**\brief Handle request
   *
   * This is the generic inbound request handler. Whenever a new request needs
   * to be handled, this will go through the registered list of regexen, and for
   * all that match it will call the registered function, until one of them
   * returns true.
   *
   * \param[out] sess The session object where the request was made.
   */
  void operator()(session &sess) const {
    std::set<std::string> methods{};
    bool trigger405 = false;
    bool methodSupported = false;

    for (auto &proc : subprocessor) {
      std::regex rx(proc.first);
      std::regex mx(proc.second.first);
      std::smatch matches;

      bool resourceMatch = std::regex_match(sess.resource, matches, rx);
      bool methodMatch = std::regex_match(sess.method, mx);

      methodSupported = methodSupported || methodMatch;

      if (resourceMatch) {
        if (methodMatch) {
          if (proc.second.second(sess, matches)) {
            return;
          }
          methods.insert(sess.method);
        } else
          for (const auto &m : http::method) {
            if (std::regex_match(m, mx)) {
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

    if (trigger405 && (methods.size() > 0)) {
      std::string allow = "";
      for (const auto &m : methods) {
        allow += (allow == "" ? "" : ",") + m;
      }

      sess.reply(405, {{"Allow", allow}},
                 "Sorry, this resource is not available via this method.");
      return;
    }

    sess.reply(404, "Sorry, this resource was not found.");
  }

  /**\brief Add handler
   *
   * This function adds a handler for a specific regex. Currently this is stored
   * in a map, so the order is probably unpredictable - but also probably just
   * alphabetic.
   *
   * \param[in]  rx      The regex that should trigger a given handler.
   * \param[out] handler The function to call.
   * \param[in]  methodx Regex for allowed methods.
   */
  void add(const std::string &rx,
           std::function<bool(session &, std::smatch &)> handler,
           const std::string &methodx = "GET") {
    subprocessor[rx] = {std::regex(methodx), handler};
  }

  /**\brief Begin handling requests
   *
   * Called by a specific session object to indicate that a session and
   * connection have now been established. Doesn't do anything in the server
   * case.
   */
  void start(session &) const {}

 protected:
  /**\brief Map of request handlers
   *
   * This is the map that holds the request handlers. It maps regex strings to
   * handler functions, which is fairly straightforward.
   */
  std::map<std::string,
           std::pair<std::regex, std::function<bool(session &, std::smatch &)>>>
      subprocessor;
};

template <class sock>
class client {
 public:
  /**\brief Session type
   *
   * This is the session type that the processor is intended for. This typedef
   * is mostly here for convenience.
   */
  typedef session<sock, client<sock>> session;

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

 protected:
  class request {
   public:
    std::string method;
    std::string resource;
    headers header;
    std::string body;

    request(const std::string &pMethod, const std::string &pResource,
            const headers &pHeader, const std::string &pBody)
        : method(pMethod), resource(pResource), header(pHeader), body(pBody){};
  };

 public:
  client &query(const std::string &method, const std::string &resource,
                const headers &header, const std::string &body) {
    requests.push_back(request(method, resource, header, body));
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

template <typename base, typename requestProcessor = processor::base<base>>
using connection = net::connection<requestProcessor>;

/**\brief Session wrapper
 *
 * Used by the server to keep track of all the data associated with a single,
 * asynchronous client connection.
 *
 * \tparam requestProcessor The functor class to handle requests.
 */
template <typename base, typename requestProcessor>
class session {
 protected:
  /**\brief Connection type
   *
   * This is the type of the server or client that the session is being served
   * on; used when instantiating a session, as we need to use some of the data
   * the server or client may have to offer.
   */
  using connectionType = connection<base, requestProcessor>;

  /**\brief HTTP request parser status
   *
   * Contains the current status of the request parser for the current session.
   */
  enum {
    stRequest,    /**< Waiting for a request line. */
    stStatus,     /**< Waiting for a status line. */
    stHeader,     /**< Currently parsing the request header. */
    stContent,    /**< Currently parsing the request body. */
    stProcessing, /**< Currently processing the request. */
    stErrorContentTooLarge,
    /**< Error: Content-Length is greater than maxContentLength */
    stShutdown /**< Will shut down the connection now. Set in the destructor. */
  } status;

 public:
  std::shared_ptr<session> self;

  /**\brief Stream socket
   *
   * This is the asynchronous I/O socket that is used to communicate with the
   * client.
   */
  typename base::socket socket;

  /**\brief The request's HTTP method
   *
   * Contains the HTTP method that was used in the request, e.g. GET, HEAD,
   * POST, PUT, etc.
   */
  std::string method;

  /**\brief Requested HTTP resource location
   *
   * This is the location that was requested, e.g. / or /fortune or something
   * like that.
   */
  std::string resource;

  /**\brief Request protocol and version identifier
   *
   * The HTTP request line contains the protocol to use for communication, which
   * is stored in this string.
   */
  std::string protocol;

  /**\brief The string to idenfity this server or client
   *
   * Used either as the Server or the User-Agent header field, depending on
   * whether this is a reply or request.
   *
   * If this is an empty string, then generating these fields is dropped.
   */
  std::string agent;

  /**\brief Status code
   *
   * HTTP replies send a status code to indicate errors and the like. For client
   * request replies, this code is set based on what the client sent.
   */
  int code;

  /**\brief Status description
   *
   * HTTP replies have a textual description of the status code that was sent;
   * this member variable holds that description.
   */
  std::string description;

  /**\brief HTTP request headers
   *
   * Contains all the headers that were sent along with the request. Things
   * like Host: or DNT:. Uses a special case-insensitive sorting function for
   * the map, so that you can query the contents without having to know the case
   * of the headers as they were sent.
   */
  headers header;

  /**\brief HTTP request body
   *
   * Contains the request body, if the request contained one.
   */
  std::string content;

  /**\brief Connection instance
   *
   * A reference to the server or client that this session belongs to and was
   * spawned from. Used to process requests and potentially for general
   * maintenance.
   */
  connectionType &connection;

  /**\brief Maximum request content size
   *
   * The maximum number of octets supported for a request body. Requests larger
   * than this are cancelled with an error.
   */
  std::size_t maxContentLength = (1024 * 1024 * 12);

  /**\brief Construct with I/O connection
   *
   * Constructs a session with the given asynchronous connection.
   *
   * \param[in] pConnection The connection instance this session belongs to.
   */
  session(connectionType &pConnection)
      : self(this),
        connection(pConnection),
        socket(pConnection.io.get()),
        status(stRequest),
        input(),
        agent(cxxhttp::identifier) {}

  /**\brief Destructor
   *
   * Closes the socket, cancels all remaining requests and sets the status to
   * stShutdown.
   */
  ~session(void) {
    status = stShutdown;

    try {
      socket.shutdown(base::socket::shutdown_both);
    } catch (std::system_error &e) {
      std::cerr << "exception while shutting down socket: " << e.what() << "\n";
    }

    try {
      socket.close();
    } catch (std::system_error &e) {
      std::cerr << "exception while closing socket: " << e.what() << "\n";
    }
  }

  /**\brief Start processing
   *
   * Starts processing the incoming request.
   */
  void start() {
    connection.processor.start(*this);
    read();
  }

  /**\brief Send reply
   *
   * Used by the processing code once it is known how to answer the request
   * contained in this object. Headers are not checked for validity.
   * This function does not add a Content-Length, so the free-form header field
   * will need to include this.
   *
   * \note The code will always reply with an HTTP/1.1 reply, regardless of the
   *       version in the request. If this is a concern for you, put the server
   *       behind an nginx instance, which should fix up the output as
   *       necessary.
   *
   * \param[in] status The status to return.
   * \param[in] header Any additional headers to be sent.
   * \param[in] body   The response body to send back to the client.
   */
  void replyFlat(int status, const std::string &header,
                 const std::string &body) {
    std::stringstream reply;
    std::string statusDescr = "Other Status";
    auto it = http::status.find(status);
    if (it != http::status.end()) {
      statusDescr = it->second;
    }

    reply << "HTTP/1.1 " << status << " " + statusDescr + "\r\n"
          << header << "\r\n" + body;

    asio::async_write(socket, asio::buffer(reply.str()),
                      [status, this](std::error_code ec, std::size_t length) {
                        handleWrite(status, ec);
                      });

    connection.log << net::address(socket) << " - - [-] \"" << method << " "
                   << resource << " " << protocol << "\" " << status << " "
                   << body.length() << " \"-\" \"-\"\n";
  }

  void request(const std::string &method, const std::string &resource,
               headers header, const std::string &body) {
    if (agent != "") {
      header["User-Agent"] = agent;
    }

    std::string req = method + " " + resource + " HTTP/1.1\r\n" +
                      flatten(header) + "\r\n" + body;

    if (status == stRequest) {
      status = stStatus;
    }

    asio::async_write(
        socket, asio::buffer(req),
        [&](std::error_code ec, std::size_t length) { handleWrite(0, ec); });
  }

  /**\brief Send reply with custom header map.
   *
   * This function will automatically add a Content-Length header for the body,
   * and will also append to the Server header, if the agent string is set.
   *
   * Further headers may be added using the header parameter. Headers are not
   * checked for validity.
   *
   * Actually sending the relpy is done via the flat-header form of this
   * function.
   *
   * \param[in] status The status to return.
   * \param[in] header The headers to send.
   * \param[in] body   The response body to send back to the client.
   */
  void reply(int status, headers header, const std::string &body) {
    header["Content-Length"] = std::to_string(body.size());

    if (agent != "") {
      if (header["Server"] != "") {
        header["Server"] += " ";
      }
      header["Server"] += agent;
    }

    /* we automatically close connections when an error code is sent. */
    if (status >= 400) {
      header["Connection"] = "close";
    }

    replyFlat(status, flatten(header), body);
  }

  /**\brief Send reply without custom headers
   *
   * Wraps around the 3-parameter reply() function, so that you don't have to
   * specify an empty header parameter if you don't intend to set custom
   * headers.
   *
   * \param[in] status The status to return.
   * \param[in] body   The response body to send back to the client.
   */
  void reply(int status, const std::string &body) { reply(status, {}, body); }

 protected:
  /**\brief Read more data
   *
   * Called by ASIO to indicate that new data has been read and can now be
   * processed.
   *
   * The actual processing for the header is done with a set of regexen, which
   * greatly simplifies the header parsing.
   *
   * \param[in] error             Current error state.
   * \param[in] bytes_transferred The amount of data that has been read.
   */
  void handleRead(const std::error_code &error, size_t bytes_transferred) {
    if (status == stShutdown) {
      return;
    }

    if (error) {
      self.reset();
      return;
    }

    static const std::regex req(
        "(\\w+)\\s+([\\w\\d%/.:;()+-]+|\\*)\\s+(HTTP/1.[01])\\s*");
    static const std::regex stat("(HTTP/1.[01])\\s+([0-9]{3})\\s+(.*)\\s*");
    static const std::regex mime("([\\w-]+):\\s*(.*)\\s*");
    static const std::regex mimeContinued("[ \t]\\s*(.*)\\s*");

    std::istream is(&input);
    std::string s;

    std::smatch matches;

    switch (status) {
      case stRequest:
      case stStatus:
      case stHeader:
        std::getline(is, s);
        break;
      case stContent:
        s = std::string(input.size(), '\0');
        is.read(&s[0], input.size());
        break;
      case stProcessing:
      case stErrorContentTooLarge:
      case stShutdown:
        break;
    }

    switch (status) {
      case stRequest:
        if (std::regex_match(s, matches, req)) {
          method = matches[1];
          resource = matches[2];
          protocol = matches[3];

          header = {};
          status = stHeader;
        }
        break;

      case stStatus:
        if (std::regex_match(s, matches, stat)) {
          protocol = matches[1];
          try {
            code = std::stoi(matches[2]);
          } catch (...) {
            code = 500;
          }
          description = matches[3];

          header = {};
          status = stHeader;
        }
        break;

      case stHeader:
        if ((s == "\r") || (s == "")) {
          const auto &cli = header.find("Content-Length");

          // everything we already read before we got here that is left in the
          // buffer is part of the content.
          content = std::string(input.size(), '\0');
          is.read(&content[0], input.size());

          if (cli != header.end()) {
            try {
              contentLength = std::stoi(cli->second);
            } catch (...) {
              contentLength = 0;
            }

            if (contentLength > maxContentLength) {
              status = stErrorContentTooLarge;
              reply(413, "Request Entity Too Large");
            } else {
              status = stContent;
            }
            break;
          } else {
            status = stContent;
            s = "";
          }
        } else if (std::regex_match(s, matches, mimeContinued)) {
          header[lastHeader] += "," + std::string(matches[1]);
          break;
        } else if (std::regex_match(s, matches, mime)) {
          lastHeader = matches[1];

          // RFC 2616, section 4.2:
          // Header fields that occur multiple times must combinable into a
          // single
          // value by appending the fields in the order they occur, using commas
          // to separate the individual values.
          if (header[matches[1]] == "") {
            header[matches[1]] = matches[2];
          } else {
            header[matches[1]] += "," + std::string(matches[2]);
          }
          break;
        }

      case stContent:
        content += s;
        status = stProcessing;

        /* processing the request takes places here */
        connection.processor(*this);

        break;

      case stProcessing:
      case stErrorContentTooLarge:
      case stShutdown:
        break;
    }

    switch (status) {
      case stRequest:
      case stStatus:
      case stHeader:
      case stContent:
        read();
      case stProcessing:
      case stErrorContentTooLarge:
      case stShutdown:
        break;
    }
  }

  /**\brief Asynchronouse write handler
   *
   * Decides whether or not things need to be written to the stream, or if
   * things need to be read instead.
   *
   * Automatically deletes the object on errors - which also closes the
   * connection automagically.
   *
   * \param[in] statusCode The HTTP status code of the reply.
   * \param[in] error      Current error state.
   */
  void handleWrite(int statusCode, const std::error_code error) {
    if (status == stShutdown) {
      return;
    }

    if (error || (statusCode >= 400)) {
      self.reset();
    } else if (status == stProcessing) {
      status = stRequest;
      read();
    }
  }

  /**\brief Asynchronouse read handler
   *
   * Decides if things need to be read in and - if so - what needs to be read.
   *
   * Automatically deletes the object on errors - which also closes the
   * connection automagically.
   */
  void read(void) {
    switch (status) {
      case stRequest:
      case stStatus:
      case stHeader:
        asio::async_read_until(
            socket, input, "\n",
            [&](const asio::error_code &error, std::size_t bytes_transferred) {
              handleRead(error, bytes_transferred);
            });
        break;
      case stContent:
        asio::async_read(
            socket, input,
            asio::transfer_at_least(contentLength - content.size()),
            [&](const asio::error_code &error, std::size_t bytes_transferred) {
              handleRead(error, bytes_transferred);
            });
        break;
      case stProcessing:
      case stErrorContentTooLarge:
      case stShutdown:
        break;
    }
  }

  /**\brief Name of the last parsed header
   *
   * This is the name of the last header line that was parsed. Used with
   * multi-line headers.
   */
  std::string lastHeader;

  /**\brief Content length
   *
   * This is the value of the Content-Length header. Used when parsing a request
   * with a body.
   */
  std::size_t contentLength;

  /**\brief ASIO input stream buffer
   *
   * This is the stream buffer that the object is reading from.
   */
  asio::streambuf input;
};

template <typename base, typename requestProcessor = processor::base<base>>
using server = net::server<base, requestProcessor, session>;

template <typename base, typename requestProcessor = processor::client<base>>
using client = net::client<base, requestProcessor, session>;
}
}
}

#endif

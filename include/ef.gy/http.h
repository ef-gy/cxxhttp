/**\file
 * \brief asio.hpp HTTP Server
 *
 * An asynchronous HTTP server implementation using asio.hpp and std::regex.
 *
 * \copyright
 * Copyright (c) 2012-2015, ef.gy Project Members
 * \copyright
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * \copyright
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * \copyright
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * \see Project Documentation: http://ef.gy/documentation/libefgy
 * \see Project Source Code: https://github.com/ef-gy/libefgy
 */

#if !defined(EF_GY_HTTP_H)
#define EF_GY_HTTP_H

#include <map>
#include <regex>
#include <system_error>
#include <algorithm>
#include <functional>
#include <deque>

#include <ef.gy/server.h>
#include <ef.gy/client.h>

namespace efgy {
namespace net {
/**\brief HTTP handling
 *
 * Contains an HTTP server and templates for session management and
 * processing by user code.
 */
namespace http {

template <typename base, typename requestProcessor> class session;

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
template <class sock> class base {
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
   *
   * \returns true (if successful; but always, really).
   */
  bool operator()(session &sess) const {
    bool badMethod = false;

    for (auto &proc : subprocessor) {
      std::regex rx(proc.first);
      std::smatch matches;

      if (std::regex_match(sess.resource, matches, rx)) {
        std::regex mx(proc.second.first);

        if (std::regex_match(sess.method, mx)) {
          if (proc.second.second(sess, matches)) {
            return true;
          }
        } else if (!badMethod) {
          badMethod = true;
        }
      }
    }

    if (badMethod) {
      sess.reply(405, "Sorry, this resource is not available via this method.");
    } else {
      sess.reply(404, "Sorry, this resource was not found.");
    }

    return true;
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
   *
   * \returns A reference to *this, so you can chain calls.
   */
  base &add(const std::string &rx,
            std::function<bool(session &, std::smatch &)> handler,
            const std::string &methodx = "GET") {
    subprocessor[rx] = {methodx, handler};
    return *this;
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
  std::map<
      std::string,
      std::pair<std::string, std::function<bool(session &, std::smatch &)>>>
      subprocessor;
};

template <class sock> class client {
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

    sess.request(req.method, req.resource, req.headers, req.body);
  }

protected:
  class request {
  public:
    std::string method;
    std::string resource;
    std::string headers;
    std::string body;

    request(const std::string &pMethod, const std::string &pResource,
            const std::string &pHeaders, const std::string &pBody)
        : method(pMethod), resource(pResource), headers(pHeaders),
          body(pBody){};
  };

public:
  client &query(const std::string &method, const std::string &resource,
                const std::string &headers, const std::string &body) {
    requests.push_back(request(method, resource, headers, body));
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
template <typename base, typename requestProcessor> class session {
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

  /**\brief Case-insensitive comparison functor
   *
   * A simple functor used by the attribute map to compare strings without
   * caring for the letter case.
   */
  class caseInsensitiveLT
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
  std::map<std::string, std::string, caseInsensitiveLT> header;

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
      : self(this), connection(pConnection), socket(pConnection.io.get()),
        status(stRequest), input() {}

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
   * contained in this object. The code will automatically add a correct
   * Content-Length header, but further headers may be added using the free-form
   * header parameter. Headers are not checked for validity.
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
  void reply(int status, const std::string &header, const std::string &body) {
    std::stringstream reply;

    reply << "HTTP/1.1 " << status << " NA\r\n"
          << "Content-Length: " << body.length() << "\r\n";

    /* we automatically close connections when an error code is sent. */
    if (status >= 400) {
      reply << "Connection: close\r\n";
    }

    reply << header << "\r\n" + body;

    asio::async_write(socket, asio::buffer(reply.str()),
                      [&](std::error_code ec, std::size_t length) {
                        handleWrite(status, ec);
                      });

    connection.log << net::address(socket) << " - - [-] \"" << method << " "
                   << resource << " " << protocol << "\" " << status << " "
                   << body.length() << " \"-\" \"-\"\n";
  }

  void request(const std::string &method, const std::string &resource,
               const std::string &header, const std::string &body) {
    std::string req =
        method + " " + resource + " HTTP/1.1\r\n" + header + "\r\n" + body;

    if (status == stRequest) {
      status = stStatus;
    }

    asio::async_write(
        socket, asio::buffer(req),
        [&](std::error_code ec, std::size_t length) { handleWrite(0, ec); });
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
  void reply(int status, const std::string &body) { reply(status, "", body); }

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
    }

    static const std::regex req(
        "(\\w+)\\s+([\\w\\d%/.:;()+-]+)\\s+(HTTP/1.[01])\\s*");
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

        header = std::map<std::string, std::string, caseInsensitiveLT>();
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

        header = std::map<std::string, std::string, caseInsensitiveLT>();
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
            reply(400, "Request body too large");
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
        // Header fields that occur multiple times must combinable into a single
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
  void handleWrite(int statusCode, const std::error_code &error) {
    if (status == stShutdown) {
      return;
    }

    if (error || (statusCode >= 400)) {
      self.reset();
    }

    if (status == stProcessing) {
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

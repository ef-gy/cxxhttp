/* asio.hpp-assisted HTTP protocol handling
 *
 * An asynchronous HTTP server implementation using asio.hpp and std::regex.
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
#if !defined(CXXHTTP_HTTP_H)
#define CXXHTTP_HTTP_H

#include <algorithm>
#include <functional>
#include <regex>
#include <system_error>

#include <cxxhttp/client.h>
#include <cxxhttp/negotiate.h>
#include <cxxhttp/server.h>

#include <cxxhttp/http-constants.h>
#include <cxxhttp/http-header.h>
#include <cxxhttp/http-processor.h>
#include <cxxhttp/http-request.h>
#include <cxxhttp/http-status.h>

namespace cxxhttp {
namespace http {
/* Session wrapper
 * @transport The connection type, e.g. transport::tcp.
 * @requestProcessor The functor class to handle requests.
 *
 * Used by the server to keep track of all the data associated with a single,
 * asynchronous client connection.
 */
template <typename transport, typename requestProcessor>
class session {
 public:
  /* Connection type.
   *
   * This is the type of the server or client that the session is being served
   * on; used when instantiating a session, as we need to use some of the data
   * the server or client may have to offer.
   */
  using connectionType = net::connection<requestProcessor>;

  /* Connection socket type.
   *
   * The type of socket we're talking to, which is the transport's socket type.
   */
  using socketType = typename transport::socket;

  /* Current status of the session.
   *
   * Used to determine what sort of communication to expect next.
   */
  enum status status;

  /* Stream socket
   *
   * This is the asynchronous I/O socket that is used to communicate with the
   * client.
   */
  socketType socket;

  /* The request's HTTP method
   *
   * Contains the HTTP method that was used in the request, e.g. GET, HEAD,
   * POST, PUT, etc.
   */
  std::string method;

  /* Requested HTTP resource location
   *
   * This is the location that was requested, e.g. / or /fortune or something
   * like that.
   */
  std::string resource;

  /* Request protocol and version identifier
   *
   * The HTTP request line contains the protocol to use for communication, which
   * is stored in this string.
   */
  std::string protocol;

  /* The reply's status line.
   *
   * Only set if for an HTTP client, not a server.
   */
  statusLine replyStatus;

  /* HTTP request headers
   *
   * Contains all the headers that were sent along with the request. Things
   * like Host: or DNT:. Uses a special case-insensitive sorting function for
   * the map, so that you can query the contents without having to know the case
   * of the headers as they were sent.
   */
  headers header;

  /* Automatically negotiated headers.
   *
   * Set with the value of automatically negotiated headers, if such
   * negotiation has taken places. This uses the input header names, e.g.
   * "Accept" instead of "Content-Type".
   */
  headers negotiated;

  /* Contains headers that will automatically be sent.
   *
   * This contains headers that were either set through the environment, or via
   * content negotiation, and which will be sent to the client with the reply
   * method.
   *
   * For negotiated headers, this uses the output header name, e.g.
   * "Content-Type" for when the client used "Accept".
   */
  parser<headers> outbound;

  /* HTTP request body
   *
   * Contains the request body, if the request contained one.
   */
  std::string content;

  /* Content length
   *
   * This is the value of the Content-Length header. Used when parsing a request
   * with a body.
   */
  std::size_t contentLength;

  /* Connection instance
   *
   * A reference to the server or client that this session belongs to and was
   * spawned from. Used to process requests and potentially for general
   * maintenance.
   */
  connectionType &connection;

  /* Construct with I/O connection
   * @pConnection The connection instance this session belongs to.
   *
   * Constructs a session with the given asynchronous connection.
   */
  session(connectionType &pConnection)
      : connection(pConnection),
        socket(pConnection.io),
        status(stRequest),
        input() {}

  /* Destructor.
   *
   * Closes the socket, cancels all remaining requests and sets the status to
   * stShutdown.
   */
  ~session(void) {
    status = stShutdown;

    try {
      socket.shutdown(socketType::shutdown_both);
    } catch (std::system_error &e) {
      std::cerr << "exception while shutting down socket: " << e.what() << "\n";
    }

    try {
      socket.close();
    } catch (std::system_error &e) {
      std::cerr << "exception while closing socket: " << e.what() << "\n";
    }
  }

  /* Start processing.
   *
   * Starts processing the incoming request.
   */
  void start(void) { connection.processor.start(*this); }

  /* Send reply
   * @status The status to return.
   * @header Any additional headers to be sent.
   * @body The response body to send back to the client.
   *
   * Used by the processing code once it is known how to answer the request
   * contained in this object. Headers are not checked for validity.
   * This function does not add a Content-Length, so the free-form header field
   * will need to include this.
   *
   * The code will always reply with an HTTP/1.1 reply, regardless of the
   * version in the request. If this is a concern for you, put the server behind
   * an nginx instance, which should fix up the output as necessary.
   */
  void replyFlat(int status, const std::string &header,
                 const std::string &body) {
    std::stringstream reply;
    statusLine stat(status);

    reply << std::string(stat) << header << "\r\n";

    if (status == 100) {
      // informational response, no body.
    } else {
      reply << body;
    }

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
    parser<headers> head{header};
    head.insert(defaultClientHeaders);

    std::string req = method + " " + resource + " HTTP/1.1\r\n" +
                      std::string(head) + "\r\n" + body;

    if (status == stRequest) {
      status = stStatus;
    }

    asio::async_write(
        socket, asio::buffer(req),
        [&](std::error_code ec, std::size_t length) { handleWrite(0, ec); });
  }

  /* Send reply with custom header map.
   * @status The status to return.
   * @header The headers to send.
   * @body The response body to send back to the client.
   *
   * This function will automatically add a Content-Length header for the body,
   * and will also append to the Server header, if the agent string is set.
   *
   * Further headers may be added using the header parameter. Headers are not
   * checked for validity.
   *
   * Actually sending the relpy is done via the flat-header form of this
   * function.
   */
  void reply(int status, const headers &header, const std::string &body) {
    parser<headers> head{{
        {"Content-Length", std::to_string(body.size())},
    }};

    if (status == 100) {
      // informational response, no body.
      head = {};
    }

    // Add the headers the client wanted to send.
    head.insert(header);

    // take over outbound headers that have been negotiated, or similar, iff
    // they haven't been overridden.
    head.insert(outbound.header);

    // we automatically close connections when an error code is sent.
    if (status >= 400) {
      head.header["Connection"] = "close";
    }

    replyFlat(status, std::string(head), body);
  }

  /* Send reply without custom headers
   * @status The status to return.
   * @body The response body to send back to the client.
   *
   * Wraps around the 3-parameter reply() function, so that you don't have to
   * specify an empty header parameter if you don't intend to set custom
   * headers.
   */
  void reply(int status, const std::string &body) { reply(status, {}, body); }

  /* Asynchronouse read handler
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
      case stError:
      case stShutdown:
        break;
    }
  }

 protected:
  /* Read more data
   * @error Current error state.
   * @bytes_transferred The amount of data that has been read.
   *
   * Called by ASIO to indicate that new data has been read and can now be
   * processed.
   *
   * The actual processing for the header is done with a set of regexen, which
   * greatly simplifies the header parsing.
   */
  void handleRead(const std::error_code &error, size_t bytes_transferred) {
    if (status == stShutdown) {
      return;
    }

    if (error) {
      delete this;
      return;
    }

    std::istream is(&input);
    std::string s;

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
      case stError:
      case stShutdown:
        break;
    }

    switch (status) {
      case stRequest: {
        const auto req = requestLine(s);
        if (req.valid()) {
          method = req.method;
          resource = req.resource;
          protocol = req.protocol();

          headerParser = {};
          status = stHeader;
        }
      } break;

      case stStatus: {
        const auto stat = statusLine(s);
        if (stat.valid()) {
          replyStatus = stat;
          protocol = stat.protocol();

          headerParser = {};
          status = stHeader;
        }
      } break;

      case stHeader:
        if ((s == "\r") || (s.empty())) {
          header = headerParser.header;
          status = connection.processor.afterHeaders(*this);
          if (contentLength > 0) {
            // read in up to Content-Length bytes from the stream from what's
            // already available.
            std::size_t readNow = std::min(input.size(), contentLength);
            content = std::string(readNow, '\0');
            is.read(&content[0], readNow);
            break;
          } else {
            content = "";
            s = "";
          }
        } else {
          headerParser.absorb(s);
          break;
        }

      case stContent:
        content += s;
        status = stProcessing;

        /* processing the request takes places here */
        connection.processor.handle(*this);

        break;

      case stProcessing:
      case stError:
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
      case stError:
      case stShutdown:
        break;
    }
  }

  /* Asynchronouse write handler
   * @statusCode The HTTP status code of the reply.
   * @error Current error state.
   *
   * Decides whether or not things need to be written to the stream, or if
   * things need to be read instead.
   *
   * Automatically deletes the object on errors - which also closes the
   * connection automagically.
   */
  void handleWrite(int statusCode, const std::error_code error) {
    if (status == stShutdown) {
      return;
    }

    if (error || (statusCode >= 400)) {
      delete this;
    } else if (status == stProcessing) {
      status = stRequest;
      read();
    }
  }

  /* Header parser instance.
   *
   * Constains all the state we need to parse headers.
   */
  parser<headers> headerParser;

  /* ASIO input stream buffer
   *
   * This is the stream buffer that the object is reading from.
   */
  asio::streambuf input;
};

template <class transport,
          class requestProcessor = processor::server<transport>>
using server = net::server<transport, requestProcessor, session>;

template <class transport,
          class requestProcessor = processor::client<transport>>
using client = net::client<transport, requestProcessor, session>;
}
}

#endif

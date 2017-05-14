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

#include <system_error>

#include <cxxhttp/client.h>
#include <cxxhttp/server.h>

#include <cxxhttp/http-processor.h>

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
class session : public sessionData {
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

  /* Stream socket
   *
   * This is the asynchronous I/O socket that is used to communicate with the
   * client.
   */
  socketType socket;

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
      : connection(pConnection), socket(pConnection.io), sessionData() {
    connection.sessions.insert(this);
  }

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

    connection.sessions.erase(this);
  }

  /* Start processing.
   *
   * Starts processing the incoming request.
   */
  void start(void) { connection.processor.start(*this); }

  /* Send request.
   * @method The request method.
   * @resource Which request to get.
   * @header Any headers to send.
   * @body A request body, if applicable.
   *
   * Sends a request to whatever this session is connected to. Only really makes
   * sense if this is a client, but nobody's preventing you from doing your own
   * thing.
   */
  void request(const std::string &method, const std::string &resource,
               headers header, const std::string &body = "") {
    parser<headers> head{header};
    head.insert(defaultClientHeaders);

    std::string req = std::string(requestLine(method, resource)) +
                      std::string(head) + "\r\n" + body;

    if (status == stRequest) {
      status = stStatus;
    }

    asio::async_write(
        socket, asio::buffer(req),
        [&](std::error_code ec, std::size_t length) { handleWrite(0, ec); });

    requests++;
  }

  /* Send reply with custom header map.
   * @status The status to return.
   * @header The headers to send.
   * @body The response body to send back to the client.
   *
   * Used by the processing code once it is known how to answer the request
   * contained in this object. Headers are not checked for validity.
   * This function does not add a Content-Length, so the free-form header field
   * will need to include this.
   *
   * This function will automatically add a Content-Length header for the body,
   * and will also append to the Server header, if the agent string is set.
   *
   * Further headers may be added using the header parameter. Headers are not
   * checked for validity.
   *
   * The code will always reply with an HTTP/1.1 reply, regardless of the
   * version in the request. If this is a concern for you, put the server behind
   * an nginx instance, which should fix up the output as necessary.
   */
  void reply(int status, const headers &header, const std::string &body) {
    parser<headers> head{{
        {"Content-Length", std::to_string(body.size())},
    }};

    if (status < 200) {
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

    std::stringstream reply;

    reply << std::string(statusLine(status)) << std::string(head) << "\r\n";

    if (status < 200) {
      // informational response, no body.
    } else {
      reply << body;
    }

    asio::async_write(socket, asio::buffer(reply.str()),
                      [status, this](std::error_code ec, std::size_t length) {
                        handleWrite(status, ec);
                      });

    static const std::regex agent("(" + grammar::token + "|[ ()/;])+");
    std::string referer = "-";
    std::string userAgent = "-";
    auto it = this->header.find("Referer");
    if (it != this->header.end()) {
      referer = it->second;
      uri ref = referer;
      if (!ref.valid()) {
        referer = "(invalid)";
      } else {
        referer = ref;
      }
    }

    it = this->header.find("User-Agent");
    if (it != this->header.end()) {
      userAgent = it->second;
      if (!std::regex_match(userAgent, agent)) {
        userAgent = "(redacted)";
      }
    }

    connection.log << net::address(socket) << " - - [-] \""
                   << trim(inboundRequest) << "\" " << status << " "
                   << body.length() << " \"" << referer << "\" \"" << userAgent
                   << "\"\n";

    replies++;
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
              handleRead(error);
            });
        break;
      case stContent:
        if (remainingBytes() > 0) {
          asio::async_read(
              socket, input, asio::transfer_at_least(remainingBytes()),
              [&](const asio::error_code &error,
                  std::size_t bytes_transferred) { handleRead(error); });
        } else {
          handleRead(std::error_code());
        }
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
   *
   * Called by ASIO to indicate that new data has been read and can now be
   * processed.
   *
   * The actual processing for the header is done with a set of regexen, which
   * greatly simplifies the header parsing.
   */
  void handleRead(const std::error_code &error) {
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
        s = std::string(std::min(remainingBytes(), input.size()), 0);
        is.read(&s[0], std::min(remainingBytes(), input.size()));
        break;
      case stProcessing:
      case stError:
      case stShutdown:
        break;
    }

    bool doRead = true;

    switch (status) {
      case stRequest: {
        inboundRequest = s;
        if (inboundRequest.valid()) {
          headerParser = {};
          status = stHeader;
        }
        // this should have an else branch, in case the request line was not
        // valid. In that case, we ought to raise an error against the sender.
      } break;

      case stStatus: {
        inboundStatus = s;
        if (inboundStatus.valid()) {
          headerParser = {};
          status = stHeader;
        }
        // this should also have an else branch, and if it's an invalid status
        // line we should flag this to the library user.
      } break;

      case stHeader:
        headerParser.absorb(s);
        // this may return false, and if it did then what the client sent was
        // not valid HTTP and we should send back an error.
        if (headerParser.complete) {
          // we're done parsing headers, so change over to streaming in the
          // results.
          header = headerParser.header;
          status = connection.processor.afterHeaders(*this);
          content.clear();
          if (remainingBytes() > 0) {
            // read in up to Content-Length bytes from the stream from what's
            // already available.
            std::size_t readNow = std::min(input.size(), remainingBytes());
            content = std::string(readNow, 0);
            is.read(&content[0], readNow);
            break;
          } else {
            s.clear();
          }
        } else {
          break;
        }

      case stContent:
        if (!s.empty()) {
          content += s;
        }
        status = stProcessing;

        {
          std::size_t q = queries();

          /* processing the request takes place here */
          connection.processor.handle(*this);

          if (q == queries()) {
            // the processor did not send anything, so give it another chance at
            // deciding what to do with this session.
            status = connection.processor.afterProcessing(*this);
            if (status == stShutdown) {
              delete this;
              return;
            }
          }
        }

      // fall through, so as to set the doRead flag to false. If the processor
      // needed an extra read, it would've queued one itself.

      case stProcessing:
      case stError:
      case stShutdown:
        doRead = false;
        break;
    }

    if (doRead) {
      read();
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
      status = connection.processor.afterProcessing(*this);
      if (status == stShutdown) {
        delete this;
      }
    }
  }
};

/* HTTP server template.
 * @transport Transport type for the server.
 * @requestProcessor The processor to use; default to processor::server.
 *
 * This is a template for an HTTP server, based on net::server and using the
 * given processor.
 */
template <class transport,
          class requestProcessor = processor::server<transport>>
using server = net::server<transport, requestProcessor, session>;

/* HTTP client template.
 * @transport Transport type for the server.
 * @requestProcessor The processor to use; default to processor::client.
 *
 * This is a template for an HTTP client, based on net::client and using the
 * given processor.
 */
template <class transport,
          class requestProcessor = processor::client<transport>>
using client = net::client<transport, requestProcessor, session>;
}
}

#endif

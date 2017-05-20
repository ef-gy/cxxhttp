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

  /* Connection instance
   *
   * A reference to the server or client that this session belongs to and was
   * spawned from. Used to process requests and potentially for general
   * maintenance.
   */
  connectionType &connection;

  /* Stream socket
   *
   * This is the asynchronous I/O socket that is used to communicate with the
   * client.
   */
  socketType socket;

  /* Construct with I/O connection
   * @pConnection The connection instance this session belongs to.
   *
   * Constructs a session with the given asynchronous connection.
   */
  session(connectionType &pConnection)
      : connection(pConnection),
        socket(connection.io),
        beacon(*this, connection.sessions) {}

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

    std::string req = requestLine(method, resource).assemble() +
                      std::string(head) + "\r\n" + body;

    if (status == stRequest) {
      status = stStatus;
      readLine();
    }

    asio::async_write(
        socket, asio::buffer(req),
        [&](std::error_code ec, std::size_t length) { handleWrite(0, ec); });

    requests++;
  }

  /* Send reply with custom header map.
   * @status The status to return.
   * @body The response body to send back to the client.
   * @header The headers to send.
   *
   * Used by the processing code once it is known how to answer the request
   * contained in this object.
   *
   * The actual message to send is generated using the generateReply() function,
   * which receives all the parameters passed in.
   */
  void reply(int status, const std::string &body, const headers &header = {}) {
    std::string reply = generateReply(status, body, header);

    asio::async_write(socket, asio::buffer(reply),
                      [status, this](std::error_code ec, std::size_t length) {
                        handleWrite(status, ec);
                      });

    connection.log << logMessage(net::address(socket), status, body.size())
                   << "\n";

    replies++;
  }

  /* Read enough off the input socket to fill a line.
   *
   * Issue a read that will make sure there's at least one full line available
   * for processing in the input buffer.
   */
  void readLine(void) {
    asio::async_read_until(
        socket, input, "\n",
        [&](const asio::error_code &error, std::size_t bytes_transferred) {
          handleRead(error);
        });
  }

  /* Read remainder of the request body.
   *
   * Issues a read for anything left to read in the request body, if there's
   * anything left to read.
   */
  void readRemainingContent(void) {
    asio::async_read(socket, input, asio::transfer_at_least(remainingBytes()),
                     [&](const asio::error_code &error,
                         std::size_t bytes_transferred) { handleRead(error); });
  }

 protected:
  /* Session beacon.
   *
   * We want to keep track of how all of the sessions, so that we can do stats
   * over the lot of them.
   */
  efgy::beacon<session> beacon;

  /* Callback after more data has been read.
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
      status = stError;
    }

    bool wasRequest = status == stRequest;
    bool wasStart = wasRequest || status == stStatus;

    if (status == stRequest) {
      inboundRequest = buffer();
      status = inboundRequest.valid() ? stHeader : stError;
    } else if (status == stStatus) {
      inboundStatus = buffer();
      status = inboundStatus.valid() ? stHeader : stError;
    } else if (status == stHeader) {
      inbound.absorb(buffer());
      // this may return false, and if it did then what the client sent was
      // not valid HTTP and we should send back an error.
      if (inbound.complete) {
        // we're done parsing headers, so change over to streaming in the
        // results.
        status = connection.processor.afterHeaders(*this);
        content.clear();
      }
    }

    if (wasStart && status == stHeader) {
      inbound = {};
    } else if (wasRequest && status == stError) {
      // We had an edge from trying to read a request line to an error, so send
      // a message to the other end about this.
      http::error<session>(*this).reply(400);
      status = stProcessing;
    }

    if (status == stHeader) {
      readLine();
    } else if (status == stContent) {
      content += buffer();
      if (remainingBytes() == 0) {
        status = stProcessing;

        const auto q = queries();

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
      } else {
        readRemainingContent();
      }
    }

    if (status == stError) {
      delete this;
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
    if (status != stShutdown) {
      if (error || (statusCode >= 400)) {
        delete this;
      } else if (status == stProcessing) {
        status = connection.processor.afterProcessing(*this);
        if (status == stShutdown) {
          delete this;
        }
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

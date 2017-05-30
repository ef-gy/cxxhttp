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
    if (!free) {
      recycle();
    }
  }

  /* Start processing.
   *
   * Starts processing the incoming request.
   */
  void start(void) { connection.processor.start(*this); }

  /* Send the next message.
   *
   * Sends the next message in the <outboundQueue>, if there is one and no
   * message is currently in flight.
   */
  void send(void) {
    if (status != stShutdown) {
      if (!writePending) {
        if (outboundQueue.size() > 0) {
          writePending = true;
          const std::string &msg = outboundQueue.front();

          asio::async_write(socket, asio::buffer(msg),
                            [this](std::error_code ec, std::size_t length) {
                              handleWrite(ec);
                            });

          outboundQueue.pop_front();
        } else if (closeAfterSend) {
          recycle();
        }
      }
    }

    for (const auto &l : log) {
      connection.log << l << "\n";
    }

    log.clear();
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

  /* Make session reusable for future use.
   *
   * Destroys all pending data that needs to be cleaned up, and tags the session
   * as clean. This allows reusing the session, or destruction out of band.
   */
  void recycle(void) {
    status = stShutdown;

    closeAfterSend = false;
    outboundQueue.clear();

    send();

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

    free = true;
  }

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
        send();
        content.clear();
      }
    }

    if (wasStart && status == stHeader) {
      inbound = {};
    } else if (wasRequest && status == stError) {
      // We had an edge from trying to read a request line to an error, so send
      // a message to the other end about this.
      http::error(*this).reply(400);
      send();
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
        send();

        if (q == queries()) {
          // the processor did not send anything, so give it another chance at
          // deciding what to do with this session.
          status = connection.processor.afterProcessing(*this);
          send();
          if (status == stShutdown) {
            recycle();
            return;
          }
        }
      } else {
        readRemainingContent();
      }
    }

    if (status == stError) {
      recycle();
    }
  }

  /* Asynchronouse write handler
   * @error Current error state.
   *
   * Decides whether or not things need to be written to the stream, or if
   * things need to be read instead.
   *
   * Automatically deletes the object on errors - which also closes the
   * connection automagically.
   */
  void handleWrite(const std::error_code error) {
    if (status != stShutdown) {
      writePending = false;
      if (error) {
        recycle();
      } else {
        if (status == stProcessing) {
          status = connection.processor.afterProcessing(*this);
        }
        if (status == stShutdown) {
          recycle();
        } else {
          send();
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

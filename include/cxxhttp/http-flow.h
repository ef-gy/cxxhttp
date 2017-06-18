/* asio.hpp-assisted HTTP I/O control flow.
 *
 * Contains a class template for coordinating reading and writing for an HTTP
 * connection.
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
#if !defined(CXXHTTP_HTTP_FLOW_H)
#define CXXHTTP_HTTP_FLOW_H

#include <system_error>

#define ASIO_STANDALONE
#include <asio.hpp>

#include <cxxhttp/http-session.h>
#include <cxxhttp/http-error.h>

namespace cxxhttp {
namespace http {
/* Shut down a connection, if supported.
 * @T Type name for the connection, e.g. asio::ip::socket.
 * @connection What to shut down.
 * @ec Output error code.
 *
 * Initiates a full shutdown on the given connection. The default is to assume
 * that the connection has a shutdown() method, and to call it.
 */
template <typename T>
static inline void maybeShutdown(T &connection, asio::error_code &ec) {
  connection.shutdown(T::shutdown_both, ec);
}

/* Stub for a connection shutdown on a file descriptor.
 * @connection What to shut down.
 * @ec Output error code.
 *
 * Does nothing, because shutting down a connection is not supported on a file
 * descriptor.
 */
template <>
inline void maybeShutdown<asio::posix::stream_descriptor>(
    asio::posix::stream_descriptor &connection, asio::error_code &ec) {}

/* HTTP I/O control flow.
 * @requestProcessor The functor class to handle requests.
 * @inputType An ASIO-compatible type for the input stream.
 * @outputType An ASIO-compatible type for the output stream; optional.
 *
 * Instantated by a session to do the actual grunt work of deciding when to talk
 * back on the wire, and how.
 *
 * This basically provides all the flow control of streaming data in and back,
 * depending on what the processor will need.
 */
template <typename requestProcessor, typename inputType,
          typename outputType = inputType &>
class flow {
 public:
  /* Processor reference.
   *
   * Used to handle requests, whenever we've completed one.
   */
  requestProcessor &processor;

  /* Input stream descriptor.
   *
   * Will be read from, but not written to.
   */
  inputType inputConnection;

  /* Output stream descriptor.
   *
   * Will be written to, but not read from.
   */
  outputType outputConnection;

  /* Reference to the HTTP session object.
   *
   * Used with the processor to handle a request. This class used to be the
   * session, but now it's split up to improve separation of concerns here.
   */
  sessionData &session;

  /* Construct with I/O service.
   * @pProcessor Reference to the HTTP processor to use.
   * @service Which ASIO I/O service to bind to.
   * @pSession The session to use for inbound requests.
   *
   * Allows specifying the I/O service instance and nothing more. This is for
   * when the output is really supposed to be a reference to the input, such as
   * with TCP or UNIX sockets.
   */
  flow(requestProcessor &pProcessor, asio::io_service &service,
       sessionData &pSession)
      : processor(pProcessor),
        inputConnection(service),
        outputConnection(inputConnection),
        session(pSession) {}

  /* Construct with I/O service and input/output data.
   * @T Input and output connection parameter type.
   * @pProcessor Reference to the HTTP processor to use.
   * @service Which ASIO I/O service to bind to.
   * @pSession The session to use for inbound requests.
   * @pInput Passed to the input connection's constructor.
   * @pOutput Passed to the output connection's constructor.
   *
   * Allows specifying the I/O service instance and parameters for the input and
   * output connecitons. This is for when we want to be explicit about what the
   * input and output point to, such as when running over STDIO.
   */
  template <typename T>
  flow(requestProcessor &pProcessor, asio::io_service &service,
       sessionData &pSession, const T &pInput, const T &pOutput)
      : processor(pProcessor),
        inputConnection(service, pInput),
        outputConnection(service, pOutput),
        session(pSession) {}

  /* Destructor.
   *
   * Closes the descriptors, cancels all remaining requests and sets the status
   * to stShutdown.
   */
  ~flow(void) {
    if (!session.free) {
      recycle();
    }
  }

  /* Start processing.
   *
   * Starts processing the incoming request.
   */
  void start(void) {
    processor.start(session);
    if (session.status == stRequest || session.status == stStatus) {
      readLine();
    } else if (session.status == stShutdown) {
      recycle();
    }
    send();
  }

  /* Send the next message.
   *
   * Sends the next message in the <outboundQueue>, if there is one and no
   * message is currently in flight.
   */
  void send(void) {
    if (session.status != stShutdown) {
      if (!session.writePending) {
        if (session.outboundQueue.size() > 0) {
          session.writePending = true;
          const std::string &msg = session.outboundQueue.front();

          asio::async_write(outputConnection, asio::buffer(msg),
                            [this](std::error_code ec,
                                   std::size_t length) { handleWrite(ec); });

          session.outboundQueue.pop_front();
        } else if (session.closeAfterSend) {
          recycle();
        }
      }
    }
  }

  /* Read enough off the input socket to fill a line.
   *
   * Issue a read that will make sure there's at least one full line available
   * for processing in the input buffer.
   */
  void readLine(void) {
    asio::async_read_until(
        inputConnection, session.input, "\n",
        [&](const asio::error_code &error,
            std::size_t bytes_transferred) { handleRead(error); });
  }

  /* Read remainder of the request body.
   *
   * Issues a read for anything left to read in the request body, if there's
   * anything left to read.
   */
  void readRemainingContent(void) {
    asio::async_read(inputConnection, session.input,
                     asio::transfer_at_least(session.remainingBytes()),
                     [&](const asio::error_code &error,
                         std::size_t bytes_transferred) { handleRead(error); });
  }

  /* Make session reusable for future use.
   *
   * Destroys all pending data that needs to be cleaned up, and tags the session
   * as clean. This allows reusing the session, or destruction out of band.
   */
  void recycle(void) {
    processor.recycle(session);

    session.status = stShutdown;

    session.closeAfterSend = false;
    session.outboundQueue.clear();

    send();

    asio::error_code ec;

    maybeShutdown(inputConnection, ec);
    inputConnection.close(ec);

    if (&inputConnection != &outputConnection) {
      // avoid closing the descriptor twice, if one is a reference to the other.
      maybeShutdown(outputConnection, ec);
      outputConnection.close(ec);
    }

    // we should do something here with ec, but then we've already given up on
    // this connection, so meh.
    session.errors = ec ? session.errors + 1 : session.errors;

    session.input.consume(session.input.size() + 1);

    session.free = true;
  }

 protected:
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
    if (session.status == stShutdown) {
      return;
    }

    if (error) {
      session.status = stError;
    }

    bool wasRequest = session.status == stRequest;
    bool wasStart = wasRequest || session.status == stStatus;
    http::version version;
    static const http::version limVersion{2, 0};

    if (session.status == stRequest) {
      session.inboundRequest = session.buffer();
      session.status = session.inboundRequest.valid() ? stHeader : stError;
      version = session.inboundRequest.version;
    } else if (session.status == stStatus) {
      session.inboundStatus = session.buffer();
      session.status = session.inboundStatus.valid() ? stHeader : stError;
      version = session.inboundStatus.version;
    } else if (session.status == stHeader) {
      session.inbound.absorb(session.buffer());
      // this may return false, and if it did then what the client sent was
      // not valid HTTP and we should send back an error.
      if (session.inbound.complete) {
        // we're done parsing headers, so change over to streaming in the
        // results.
        session.status = processor.afterHeaders(session);
        send();
        session.content.clear();
      }
    }

    if (wasStart && session.status != stError && version >= limVersion) {
      // reject any requests with a major version over 1.x
      session.status = stError;
    }

    if (wasStart && session.status == stHeader) {
      session.inbound = {};
    } else if (wasRequest && session.status == stError) {
      // We had an edge from trying to read a request line to an error, so send
      // a message to the other end about this.
      // The error code is a 400 for a generic error or an invalid request line,
      // or a 505 if we can't handle the message framing.
      http::error(session).reply(version >= limVersion ? 505 : 400);
      send();
      session.status = stProcessing;
    }

    if (session.status == stHeader) {
      readLine();
    } else if (session.status == stContent) {
      session.content += session.buffer();
      if (session.remainingBytes() == 0) {
        session.status = stProcessing;

        /* processing the request takes place here */
        processor.handle(session);

        session.status = processor.afterProcessing(session);
        send();

        if (session.status == stShutdown) {
          recycle();
        } else if (session.status == stRequest || session.status == stStatus) {
          readLine();
        }
      } else {
        readRemainingContent();
      }
    }

    if (session.status == stError) {
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
    if (session.status != stShutdown) {
      session.writePending = false;
      if (error) {
        recycle();
      } else {
        if (session.status == stProcessing) {
          session.status = processor.afterProcessing(session);
        }
        if (session.status == stShutdown) {
          recycle();
        } else {
          send();
        }
      }
    }
  }
};
}
}

#endif

/* asio.hpp-assisted HTTP-over-stdio
 *
 * This provides an implementation of HTTP over STDIO. This would primarily be
 * useful for testing, and for running your server through inetd.
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
#if !defined(CXXHTTP_HTTP_STDIO_H)
#define CXXHTTP_HTTP_STDIO_H

#include <system_error>

#define ASIO_STANDALONE
#include <asio.hpp>

#include <cxxhttp/http-processor.h>

namespace cxxhttp {
namespace http {
namespace stdio {
/* Session wrapper
 * @requestProcessor The functor class to handle requests.
 *
 * Used by the server to keep track of all the data associated with a single,
 * asynchronous client connection on the STDIO file descriptors.
 */
template <typename requestProcessor>
class session : public sessionData {
 public:
  /* Processor instantiation.
   *
   * Used to handle requests, whenever we've completed one.
   */
  requestProcessor processor;

  /* Input stream descriptor.
   *
   * Will be read from, but not written to.
   */
  asio::posix::stream_descriptor inputDescriptor;

  /* Output stream descriptor.
   *
   * Will be written to, but not read from.
   */
  asio::posix::stream_descriptor outputDescriptor;

  /* Construct with I/O service
   * @service Which ASIO I/O service to bind to.
   *
   * Only allows specifying the I/O service, because the input and output file
   * descriptor IDs are fixed for STDIO.
   */
  session(asio::io_service &service)
      : inputDescriptor(service, 0), outputDescriptor(service, 1) {}

  /* Destructor.
   *
   * Closes the descriptors, cancels all remaining requests and sets the status
   * to stShutdown.
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
  void start(void) {
    processor.start(*this);
    if (status == stRequest || status == stStatus) {
      readLine();
    } else if (status == stShutdown) {
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
    if (status != stShutdown) {
      if (!writePending) {
        if (outboundQueue.size() > 0) {
          writePending = true;
          const std::string &msg = outboundQueue.front();

          asio::async_write(outputDescriptor, asio::buffer(msg),
                            [this](std::error_code ec,
                                   std::size_t length) { handleWrite(ec); });

          outboundQueue.pop_front();
        } else if (closeAfterSend) {
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
        inputDescriptor, input, "\n",
        [&](const asio::error_code &error,
            std::size_t bytes_transferred) { handleRead(error); });
  }

  /* Read remainder of the request body.
   *
   * Issues a read for anything left to read in the request body, if there's
   * anything left to read.
   */
  void readRemainingContent(void) {
    asio::async_read(inputDescriptor, input,
                     asio::transfer_at_least(remainingBytes()),
                     [&](const asio::error_code &error,
                         std::size_t bytes_transferred) { handleRead(error); });
  }

  /* Make session reusable for future use.
   *
   * Destroys all pending data that needs to be cleaned up, and tags the session
   * as clean. This allows reusing the session, or destruction out of band.
   */
  void recycle(void) {
    processor.recycle(*this);

    status = stShutdown;

    closeAfterSend = false;
    outboundQueue.clear();

    send();

    asio::error_code ec;

    inputDescriptor.close(ec);
    outputDescriptor.close(ec);

    // we should do something here with ec, but then we've already given up on
    // this connection, so meh.
    errors = ec ? errors + 1 : errors;

    input.consume(input.size() + 1);

    free = true;
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
        status = processor.afterHeaders(*this);
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

        /* processing the request takes place here */
        processor.handle(*this);

        status = processor.afterProcessing(*this);
        send();

        if (status == stShutdown) {
          recycle();
        } else if (status == stRequest || status == stStatus) {
          readLine();
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
          status = processor.afterProcessing(*this);
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
 *
 * A template for running an HTTP server through STDIO.
 */
using server = session<processor::server>;

/* HTTP client template.
 *
 * A template for running an HTTP client through STDIO.
 */
using client = session<processor::client>;
}
}
}

#endif

/* HTTP/1.1 I/O flow.
 *
 * Implements the input/output flow for an HTTP/1.1 (or lower) connection. This
 * flow is entirely agnostic of the kind of transport used, and is also the
 * default flow for the library.
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
#if !defined(CXXHTTP_FLOW_HTTP11_H)
#define CXXHTTP_FLOW_HTTP11_H

#include <cxxhttp/http-session.h>

namespace cxxhttp {
namespace control {
/* Controller Actions.
 *
 * Emitted by read/write flow control functions to tell the transport flow what
 * to do next on a given connection or buffer.
 */
enum action {
  /* do nothing */
  actNone = 0x00,
  /* call recycle() */
  actRecycle = 0x01,
  /* call start() */
  actStart = 0x02,
  /* call readLine() */
  actReadLine = 0x10,
  /* call readRemainingContent() */
  actReadRemainingContent = 0x11,
  /* call send() */
  actSend = 0x20,
};

/* High-level HTTP/1.1 data flow.
 * @requestProcessor The HTTP processor to call into, when appropriate.
 *
 * This is basically a state machine that implements the decision-making process
 * for HTTP/1.1 connections.
 */
template <typename requestProcessor>
class http11 {
 public:
  /* Construct with Processor and Session.
   * @pProcessor Reference to the HTTP processor to use.
   * @pSession The session to use for inbound requests.
   *
   * Since this flow is transport-agnostic, we should only need a processor and
   * the corresponding session to construct it.
   */
  http11(requestProcessor &pProcessor, http::sessionData &pSession)
      : processor_(pProcessor), session_(pSession) {}

  /* Get reference to the HTTP session object.
   *
   * @returns reference to session_.
   */
  inline http::sessionData &session(void) const { return session_; }

  /* Make session reusable for future use.
   *
   * Destroys all pending data that needs to be cleaned up, and tags the session
   * as clean. This allows reusing the session, or destruction out of band.
   */
  void recycle(void) {
    if (!session_.free) {
      processor_.recycle(session_);

      session_.status = http::stShutdown;

      session_.closeAfterSend = false;
      session_.outboundQueue.clear();
    }
  }

  /* Decide what to do after an initial setup.
   * @initial Set to true when used in the session flow constructor.
   *
   * This does what start() does after telling the processor to get going. We
   * also need this after processing an individual request.
   *
   * @returns The next actions to take.
   */
  std::vector<action> start(bool initial) {
    std::vector<action> emit = {};

    if (initial) {
      processor_.start(session_);
    }

    if (session_.status == http::stRequest ||
        session_.status == http::stStatus) {
      emit.push_back(actReadLine);
    } else if (session_.status == http::stShutdown) {
      emit.push_back(actRecycle);
    }
    emit.push_back(actSend);

    return emit;
  }

  /* Decide what to do after more data has been read.
   * @error Current error state.
   *
   * Called by a flow controller to decide what to do next.
   *
   * @returns The next actions to take.
   */
  std::vector<action> read(const std::error_code &error) {
    std::vector<action> emit = {};

    if (session_.status == http::stShutdown) {
      return emit;
    } else if (error) {
      session_.status = http::stError;
    }

    bool wasRequest = session_.status == http::stRequest;
    bool wasStart = wasRequest || session_.status == http::stStatus;
    http::version version;
    static const http::version limVersion{2, 0};

    if (session_.status == http::stRequest) {
      session_.inboundRequest = session_.bufferLine();
      session_.status =
          session_.inboundRequest.valid() ? http::stHeader : http::stError;
      version = session_.inboundRequest.version;
    } else if (session_.status == http::stStatus) {
      session_.inboundStatus = session_.bufferLine();
      session_.status =
          session_.inboundStatus.valid() ? http::stHeader : http::stError;
      version = session_.inboundStatus.version;
    } else if (session_.status == http::stHeader) {
      session_.inbound.absorb(session_.bufferLine());
      // this may return false, and if it did then what the client sent was
      // not valid HTTP and we should send back an error.
      if (session_.inbound.complete) {
        // we're done parsing headers, so change over to streaming in the
        // results.
        session_.status = processor_.afterHeaders(session_);
        emit.push_back(actSend);
        session_.content.clear();
      }
    }

    if (wasStart && session_.status != http::stError && version >= limVersion) {
      // reject any requests with a major version over 1.x
      session_.status = http::stError;
    }

    if (wasStart && session_.status == http::stHeader) {
      session_.inbound = {};
    } else if (wasRequest && session_.status == http::stError) {
      // We had an edge from trying to read a request line to an error, so send
      // a message to the other end about this.
      // The error code is a 400 for a generic error or an invalid request line,
      // or a 505 if we can't handle the message framing.
      http::error(session_).reply(version >= limVersion ? 505 : 400);
      emit.push_back(actSend);
      session_.status = http::stProcessing;
    }

    if (session_.status == http::stHeader) {
      emit.push_back(actReadLine);
    } else if (session_.status == http::stContent) {
      session_.content += session_.bufferContent();
      if (session_.remainingBytes() == 0) {
        session_.status = http::stProcessing;

        /* processing the request takes place here */
        processor_.handle(session_);

        session_.status = processor_.afterProcessing(session_);
        emit.push_back(actStart);
      } else {
        emit.push_back(actReadRemainingContent);
      }
    }

    if (session_.status == http::stError) {
      emit.push_back(actRecycle);
    }

    return emit;
  }

  /* Write handler
   * @error Current error state.
   *
   * Decides whether or not things need to be written to the stream, or if
   * things need to be read instead.
   *
   * Errors trigger a recycling event.
   *
   * @returns The next actions to take.
   */
  std::vector<action> write(const std::error_code error) {
    std::vector<action> emit = {};

    session_.writePending = false;

    if (!error) {
      if (session_.status == http::stProcessing) {
        session_.status = processor_.afterProcessing(session_);
      }
      emit.push_back(actSend);
    }
    if (error || session_.status == http::stShutdown) {
      emit.push_back(actRecycle);
    }

    return emit;
  }

 protected:
  /* Processor reference.
   *
   * Used to handle requests, whenever we've completed one.
   */
  requestProcessor &processor_;

  /* Reference to the HTTP session object.
   *
   * Used with the processor to handle a request. This class used to be the
   * session, but now it's split up to improve separation of concerns here.
   */
  http::sessionData &session_;
};
}  // namespace control
}  // namespace cxxhttp

#endif

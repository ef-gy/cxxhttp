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

#include <functional>
#include <system_error>

#define ASIO_STANDALONE
#include <cxxhttp/http-error.h>
#include <cxxhttp/http-session.h>

#include <cxxhttp/flow-http11.h>

#include <asio.hpp>

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

/* Close a connection, maybe shut it down first.
 * @T The connection type.
 * @connection The connection to manipulate.
 * @ec Output error code.
 *
 * Calls maybeShutdown() then close() on a connection.
 */
template <typename T>
static inline void closeConnection(T &connection, asio::error_code &ec) {
  maybeShutdown(connection, ec);
  connection.close(ec);
}

/* HTTP I/O tansport flow.
 * @requestProcessor The functor class to handle requests.
 * @inputType An ASIO-compatible type for the input stream.
 * @outputType An ASIO-compatible type for the output stream; optional.
 * @controlFlow The control flow for the connection, default: control::http11.
 *
 * Instantated by a session to do the actual grunt work of deciding when to talk
 * back on the wire, and how.
 *
 * This basically provides all the flow control of streaming data in and back,
 * depending on what the processor will need.
 */
template <typename requestProcessor, typename inputType,
          typename outputType = inputType &,
          typename controlFlow = control::http11<requestProcessor>>
class flow {
 public:
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

  /* A control flow object for the HTTP session.
   *
   * Used to decide what to do at each stage of processing a request.
   */
  controlFlow controller;

  /* Construct with I/O service.
   * @processor Reference to the HTTP processor to use.
   * @service Which ASIO I/O service to bind to.
   * @session The session to use for inbound requests.
   *
   * Allows specifying the I/O service instance and nothing more. This is for
   * when the output is really supposed to be a reference to the input, such as
   * with TCP or UNIX sockets.
   */
  flow(requestProcessor &processor, asio::io_service &service,
       sessionData &session)
      : inputConnection(service),
        outputConnection(inputConnection),
        controller(processor, session) {}

  /* Construct with I/O service and input/output data.
   * @T Input and output connection parameter type.
   * @processor Reference to the HTTP processor to use.
   * @service Which ASIO I/O service to bind to.
   * @session The session to use for inbound requests.
   * @pInput Passed to the input connection's constructor.
   * @pOutput Passed to the output connection's constructor.
   *
   * Allows specifying the I/O service instance and parameters for the input and
   * output connecitons. This is for when we want to be explicit about what the
   * input and output point to, such as when running over STDIO.
   */
  template <typename T>
  flow(requestProcessor &processor, asio::io_service &service,
       sessionData &session, const T &pInput, const T &pOutput)
      : inputConnection(service, pInput),
        outputConnection(service, pOutput),
        controller(processor, session) {}

  /* Get reference to the HTTP session object.
   *
   * @returns reference to the flow controller's session().
   */
  inline http::sessionData &session(void) const { return controller.session(); }

  /* Destructor.
   *
   * Closes the descriptors, cancels all remaining requests and sets the status
   * to stShutdown.
   */
  ~flow(void) { recycle(); }

  /* Decide what to do after an initial setup.
   * @initial Set to true if start() is called in the constructor.
   *
   * This does what start() does after telling the processor to get going. We
   * also need this after processing an individual request.
   */
  void start(bool initial = true) {
    actOnFlowInstructions(controller.start(initial));
  }

  /* Make session reusable for future use.
   *
   * Destroys all pending data that needs to be cleaned up, and tags the session
   * as clean. This allows reusing the session, or destruction out of band.
   */
  void recycle(void) {
    controller.recycle();

    if (!session().free) {
      asio::error_code ec;

      closeConnection(inputConnection, ec);

      if (&inputConnection != &outputConnection) {
        // avoid closing the descriptor twice, if one is a reference to the
        // other.
        closeConnection(outputConnection, ec);
      }

      // we should do something here with ec, but then we've already given up on
      // this connection, so meh.
      session().errors += ec ? 1 : 0;

      session().input.consume(session().input.size() + 1);

      session().free = true;
    }
  }

 protected:
  /* Act on flow controller instructions.
   * @actions What to do.
   *
   * The flow controller basically just queues up actions and then returns the
   * next instruction for what to do on a given connection. This function calls
   * that next instruction.
   */
  void actOnFlowInstructions(const std::vector<control::action> &actions) {
    for (const auto &action : actions) {
      switch (action) {
        case control::actRecycle:
          recycle();
          break;
        case control::actStart:
          start(false);
          break;
        case control::actReadLine:
          readLine();
          break;
        case control::actReadRemainingContent:
          readRemainingContent();
          break;
        case control::actSend:
          send();
          break;
        default:
          break;
      }
    }
  }

  /* Callback after more data has been read.
   * @error Current error state.
   * @length Length of the read; ignored.
   *
   * Called by ASIO to indicate that new data has been read and can now be
   * processed.
   *
   * The actual processing for the header is done with a set of regexen, which
   * greatly simplifies the header parsing.
   */
  void read(const std::error_code &error, std::size_t length) {
    actOnFlowInstructions(controller.read(error));
  }

  /* Read enough off the input socket to fill a line.
   *
   * Issue a read that will make sure there's at least one full line available
   * for processing in the input buffer.
   */
  void readLine(void) {
    asio::async_read_until(inputConnection, session().input, "\n",
                           std::bind(&flow::read, this, std::placeholders::_1,
                                     std::placeholders::_2));
  }

  /* Read remainder of the request body.
   *
   * Issues a read for anything left to read in the request body, if there's
   * anything left to read.
   */
  void readRemainingContent(void) {
    asio::async_read(inputConnection, session().input,
                     asio::transfer_at_least(session().remainingBytes()),
                     std::bind(&flow::read, this, std::placeholders::_1,
                               std::placeholders::_2));
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
  void write(const std::error_code error) {
    actOnFlowInstructions(controller.write(error));
  }

  /* Asynchronously write a string.
   * @msg The message to write.
   *
   * Write a full message to an output connection.
   */
  void writeMessage(const std::string &msg) {
    asio::async_write(outputConnection, asio::buffer(msg),
                      std::bind(&flow::write, this, std::placeholders::_1));
  }

  /* Send the next message.
   *
   * Sends the next message in the <outboundQueue>, if there is one and no
   * message is currently in flight.
   */
  void send(void) {
    if (session().status != stShutdown && !session().writePending) {
      if (session().outboundQueue.size() > 0) {
        session().writePending = true;
        const std::string &msg = session().outboundQueue.front();

        writeMessage(msg);

        session().outboundQueue.pop_front();
      } else if (session().closeAfterSend) {
        recycle();
      }
    }
  }
};
}  // namespace http
}  // namespace cxxhttp

#endif

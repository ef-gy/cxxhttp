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

#include <cxxhttp/http-flow.h>
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

  /* HTTP Coordinator type.
   *
   * We need one instance of this to do the actual work of talking with the
   * other end of this connection.
   */
  using flowType = http::flow<requestProcessor, asio::posix::stream_descriptor,
                              asio::posix::stream_descriptor>;

  /* HTTP Coordinator.
   *
   * Provides flow control for our HTTP session.
   */
  flowType flow;

  /* Construct with I/O service
   * @service Which ASIO I/O service to bind to.
   *
   * Only allows specifying the I/O service, because the input and output file
   * descriptor IDs are fixed for STDIO.
   */
  session(asio::io_service &service) : flow(processor, service, *this, 0, 1) {}

  /* Destructor.
   *
   * Closes the descriptors, cancels all remaining requests and sets the status
   * to stShutdown.
   */
  ~session(void) {
    if (!free) {
      flow.recycle();
    }
  }

  /* Start processing.
   *
   * Starts processing the incoming request.
   */
  void start(void) { flow.start(); }
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

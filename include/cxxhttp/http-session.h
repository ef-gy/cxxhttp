/* HTTP session data.
 *
 * Used by parts of the implementation to keep track of state all over the
 * place.
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
#if !defined(CXXHTTP_HTTP_SESSION_H)
#define CXXHTTP_HTTP_SESSION_H

#include <cxxhttp/network.h>

#include <cxxhttp/http-header.h>
#include <cxxhttp/http-request.h>
#include <cxxhttp/http-status.h>

namespace cxxhttp {
namespace http {
/* Transport-agnostic HTTP session data.
 *
 * For all the bits in an HTTP session object that do not rely on knowing the
 * correct transport type.
 */
class sessionData {
 public:
  /* Current status of the session.
   *
   * Used to determine what sort of communication to expect next.
   */
  enum status status;

  /* The inbound request line.
   *
   * Parsed version of the request line for the last request, if applicable.
   */
  requestLine inboundRequest;

  /* The inbound status line.
   *
   * Parsed version of the status line for the last request, if applicable.
   */
  statusLine inboundStatus;

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

  /* How many requests we've sent on this connection.
   *
   * Mostly for house-keeping purposes, and to keep track of whether a client
   * processor actually sent a request.
   *
   * This variable must only increase in value.
   */
  std::size_t requests;

  /* How many replies we've sent on this connection.
   *
   * Mostly for house-keeping purposes, and to keep track of whether a server
   * processor actually sent a reply.
   *
   * This variable must only increase in value.
   */
  std::size_t replies;

  /* Default constructor
   *
   * Sets up an empty data object with default values for the members that need
   * that.
   */
  sessionData(void)
      : status(stRequest), contentLength(0), requests(0), replies(0) {}

  /* Calculate number of queries from this session.
   *
   * Calculates the total number of queries that this session has sent. Inbound
   * queries are not counted.
   *
   * @return The number of queries this session answered to.
   */
  std::size_t queries(void) const { return replies + requests; }

  /* How many bytes are left to read.
   *
   * Uses the known content length and the current content buffer's size to
   * determine how much more to read.
   *
   * @return The number of bytes remaining that we'd expect in the current
   * message.
   */
  std::size_t remainingBytes(void) const {
    return contentLength - content.size();
  }

 protected:
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
}
}

#endif

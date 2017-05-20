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

#include <cxxhttp/negotiate.h>
#include <cxxhttp/network.h>

#include <cxxhttp/http-header.h>
#include <cxxhttp/http-request.h>
#include <cxxhttp/http-status.h>

namespace cxxhttp {
namespace http {
/* HTTP header negotiation map.
 *
 * Maps input header names to their equivalent outbound version.
 */
static const headers sendNegotiatedAs{
    {"Accept", "Content-Type"},
};

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

  /* Automatically negotiated headers.
   *
   * Set with the value of automatically negotiated headers, if such
   * negotiation has taken places. This uses the input header names, e.g.
   * "Accept" instead of "Content-Type".
   */
  headers negotiated;

  /* Inbound header parser instance.
   *
   * Contains all the headers that were sent along with the request. Things
   * like Host: or DNT:. Uses a special case-insensitive sorting function for
   * the map, so that you can query the contents without having to know the case
   * of the headers as they were sent.
   *
   * This object also has all the facilities to actually parse headers.
   *
   * This is for inbound headers, i.e. sent to this instance, as opposed to
   * outbound headers, i.e. those we'll be sending to a client, or server. See
   * <outbound> for those.
   */
  parser<headers> inbound;

  /* Contains headers that will automatically be sent.
   *
   * This contains headers that were either set through the environment, or via
   * content negotiation, and which will be sent to the client with the reply
   * method.
   *
   * For negotiated headers, this uses the output header name, e.g.
   * "Content-Type" for when the client used "Accept".
   *
   * This is for outbound headers, i.e. those we'll be sending to the client
   * that connected to us, or the server we connected to. For the opposite
   * direction, see <inbound>.
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

  /* Generate an HTTP reply message.
   * @status The status to return.
   * @body The response body to send back to the client.
   * @header The headers to send.
   *
   * The reply() function uses this to create the text it will send. Headers are
   * not checked for validity.
   *
   * This function will automatically add a Content-Length header for the body,
   * and will also append to the Server header, if the agent string is set.
   *
   * The code will always reply with an HTTP/1.1 reply, regardless of the
   * version in the request. If this is a concern for you, put the server behind
   * an nginx instance, which should fix up the output as necessary.
   *
   * @return The raw HTTP message to be sent.
   */
  std::string generateReply(int status, const std::string &body,
                            const headers &header = {}) {
    // informational responses have no message body.
    bool allowBody = status >= 200;
    // we automatically close connections when an error code is sent.
    bool allowKeepAlive = status < 400;

    parser<headers> head;

    if (allowBody) {
      head.insert({
          {"Content-Length", std::to_string(body.size())},
      });
    }
    if (!allowKeepAlive) {
      head.insert({
          {"Connection", "close"},
      });
    }

    // Add the headers the client wanted to send.
    head.insert(header);

    // take over outbound headers that have been negotiated, or similar, iff
    // they haven't been overridden.
    head.insert(outbound.header);

    std::string reply =
        std::string(statusLine(status)) + std::string(head) + "\r\n";

    if (allowBody) {
      reply += body;
    }

    return reply;
  }

  /* Create a log message.
   * @address The remote address to write to the log.
   * @status Reply status code.
   * @length The number of octets sent back in the reply.
   *
   * Creates a standard, nginx combined-format log message for the currently
   * active request. This can then be sent written to a physical log stream.
   *
   * @return The full log message.
   */
  std::string logMessage(const std::string &address, int status,
                         std::size_t length) const {
    static const std::regex agent("(" + grammar::token + "|[ ()/;])+");
    std::string userAgent = inbound.get("User-Agent", "-");

    uri ref = inbound.get("Referer", "-");
    std::string referer =
        ref.valid() ? std::string(ref) : std::string("(invalid)");

    if (!std::regex_match(userAgent, agent)) {
      userAgent = "(redacted)";
    }

    return address + " - - [-] \"" + inboundRequest.assemble(false) + "\" " +
           std::to_string(status) + +" " + std::to_string(length) + " \"" +
           referer + "\" \"" + userAgent + "\"";
  }

  /* Extract partial data from the session.
   *
   * This reads data that is already available in `input` and returns it as a
   * string. How much data is extracted depends on the current state. It'll
   * either be full line if we're still parsing headers, or as much of the
   * remainder of the message body we can, if we're doing that.
   *
   * @return Partial data from the `input`, as needed by the current context.
   */
  std::string buffer(void) {
    std::istream is(&input);
    std::string s;

    if (status == stRequest || status == stStatus || status == stHeader) {
      std::getline(is, s);
    } else if (status == stContent && remainingBytes() > 0) {
      s = std::string(std::min(remainingBytes(), input.size()), 0);
      is.read(&s[0], std::min(remainingBytes(), input.size()));
    }

    return s;
  }

  /* Negotiate headers for request.
   * @negotiations The set of negotiations to perform, from the servlet.
   *
   * Uses the global header negotiation facilities to set actual inbound and
   * outbound headers based on what the input request looks like.
   *
   * Note: probably doesn't make sense to call this in a client processor, but
   * it is most certainly a session-scope thing to be done.
   *
   * @return Whether or not negotiations were successful.
   */
  bool negotiate(const headers &negotiations) {
    bool badNegotiation = false;
    // reset, and perform, header value negotiation based on the servlet's specs
    // and the client data.
    negotiated = {};
    for (const auto &n : negotiations) {
      const std::string cv =
          inbound.header.count(n.first) > 0 ? inbound.header[n.first] : "";
      const std::string v = cxxhttp::negotiate(cv, n.second);

      // modify the Vary value to indicate we used this header.
      outbound.append("Vary", n.first);

      negotiated[n.first] = v;

      const auto it = sendNegotiatedAs.find(n.first);
      if (it != sendNegotiatedAs.end()) {
        outbound.header[it->second] = v;
      }

      badNegotiation = badNegotiation || v.empty();
    }

    return !badNegotiation;
  }

 protected:
  /* ASIO input stream buffer
   *
   * This is the stream buffer that the object is reading from.
   */
  asio::streambuf input;
};
}
}

#endif

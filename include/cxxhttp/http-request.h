/* HTTP request line handling.
 *
 * For parsing request lines to the best of our abilities.
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
#if !defined(CXXHTTP_HTTP_REQUEST_H)
#define CXXHTTP_HTTP_REQUEST_H

#include <array>
#include <regex>
#include <string>

#include <cxxhttp/http-grammar.h>
#include <cxxhttp/uri.h>

namespace cxxhttp {
namespace http {
/* Broken out request line.
 *
 * Contains all the data in a requst line, broken out into the relevant fields.
 */
class requestLine {
 public:
  /* Protocol version.
   *
   * Should be < {2,0}, otherwise we'll likely reject the request at a later
   * stage in the flow control mechanism.
   */
  std::array<unsigned, 2> version;

  /* The request method.
   *
   * Something like GET, HEAD, POST, PUT, OPTIONS, TRACE, etc.
   */
  std::string method;

  /* The requested resource.
   *
   * Does not have any post-processing done to it, just yet.
   */
  uri resource;

  /* Default initialiser.
   *
   * Initialise everything to be empty. Obviously this doesn't make for a valid
   * header.
   */
  requestLine(void) : version({{0, 0}}) {}

  /* Parse HTTP request line.
   * @line The (suspected) request line to parse.
   *
   * This currently only accepts HTTP/1.0 and HTTP/1.1 request lines, all others
   * will be rejected.
   */
  requestLine(const std::string &line) : version({{0, 0}}) {
    static const std::regex req("(\\w+) ([\\w\\d%/.:;()+?=&-]+|\\*) " +
                                grammar::httpVersion + "\r?\n?");
    std::smatch matches;
    bool matched = std::regex_match(line, matches, req);

    if (matched) {
      method = matches[1];
      resource = std::string(matches[2]);
      const std::string maj = matches[3];
      const std::string min = matches[4];
      unsigned majv = maj[0] - '0';
      unsigned minv = min[0] - '0';
      version = {{majv, minv}};
    }
  }

  /* Construct with method and resource.
   * @pMethod The method to set.
   * @pResource The resource to set.
   *
   * Used by code that wants to send out requests to generate a request line.
   * Defaults the minor/major version of the protocol so that outbound replies
   * are always HTTP/1.1.
   */
  requestLine(const std::string &pMethod, const std::string &pResource)
      : version({{1, 1}}), method(pMethod), resource(pResource) {}

  /* Did this request line parse correctly?
   *
   * Set to false, unless a successful parse happened (or the object has been
   * initialised directly, presumably with correct values).
   *
   * This does not consider HTTP/0.9 request lines to be valid.
   *
   * @return A boolean indicating whether or not this is a valid status line.
   */
  bool valid(void) const {
    static const std::array<unsigned, 2> minVersion{{1, 0}};
    return (version >= minVersion) && resource.valid();
  }

  /* Protocol name.
   *
   * The value is reconstructed from the parsed value, because we only accept
   * HTTP/x.x versions anyway.
   *
   * @return HTTP/1.0 or HTTP/1.1. Or anything else that grammar::httpVersion
   * accepts.
   */
  std::string protocol(void) const {
    return "HTTP/" + std::to_string(version[0]) + "." +
           std::to_string(version[1]);
  }

  /* Create request line.
   * @newline Whether to include a trailing CRLF.
   *
   * Uses whatever data we have to create a status line. In case the input would
   * not be valid, a generic status line is created.
   *
   * @return Returns the status line in a form that could be used in HTTP.
   */
  std::string assemble(const bool newline = true) const {
    const std::string trailer = newline ? "\r\n" : "";

    if (!valid()) {
      return "FAIL * HTTP/0.0" + trailer;
    }

    return method + " " + std::string(resource) + " " + protocol() + trailer;
  }
};
}
}

#endif

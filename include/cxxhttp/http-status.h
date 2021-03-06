/* HTTP status line handling.
 *
 * For looking up status descriptions and parsing your favorite status lines.
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
#if !defined(CXXHTTP_HTTP_STATUS_H)
#define CXXHTTP_HTTP_STATUS_H

#include <cxxhttp/http-constants.h>
#include <cxxhttp/http-grammar.h>
#include <cxxhttp/http-version.h>

#include <regex>

namespace cxxhttp {
namespace http {
/* Broken out status line.
 *
 * Contains all the data in a status line, broken out into the relevant fields.
 */
class statusLine {
 public:
  /* The status code.
   *
   * Anything not in the http::status map is probably an error.
   */
  unsigned code;

  /* Protocol version.
   *
   * Should be < {2,0}, otherwise we'll likely reject the request at a later
   * stage in the flow control mechanism.
   */
  http::version version;

  /* Default initialiser.
   *
   * Initialise everything to be empty. Obviously this doesn't make for a valid
   * header.
   */
  statusLine(void) : code(0) {}

  /* Parse HTTP status line.
   * @line The (suspected) status line to parse.
   *
   * This currently only accepts HTTP/1.0 and HTTP/1.1 status lines, all others
   * will be rejected.
   */
  statusLine(const std::string &line) : statusLine() {
    static const std::regex stat(grammar::httpVersion + " (" +
                                 grammar::statusCode + ") (" +
                                 grammar::reasonPhrase + ")\r?\n?");
    std::smatch matches;
    bool matched = std::regex_match(line, matches, stat);

    if (matched) {
      const std::string maj = matches[1];
      const std::string min = matches[2];
      version = {maj, min};
      description = matches[4];
      // we pre-validate that this is a number in the range of 100-999 with the
      // regex, so while this could ordinarily throw an exception it would be
      // programme-terminatingly bad if it did here.
      code = std::stoi(matches[3]);
    }
  }

  /* Create status line with status and protocol.
   * @pStatus The status code.
   * @pVersion Protocl version pair, defaults to 1.1.
   *
   * Use this to create a status line when replying to a query.
   */
  statusLine(unsigned pStatus, const http::version &pVersion = {1, 1})
      : code(pStatus),
        version(pVersion),
        description(getDescription(pStatus)) {}

  /* Did this status line parse correctly?
   *
   * Set to false, unless a successful parse happened (or the object has been
   * initialised directly, presumably with correct values).
   *
   * This does not consider anything before HTTP/0.9 status lines to be valid.
   *
   * @return A boolean indicating whether or not this is a valid status line.
   */
  bool valid(void) const {
    return (code >= 100) && (code < 600) && version.valid();
  }

  /* Protocol name.
   *
   * Reconstructed from the parsed value. We only accept HTTP/x.x protocol
   * versions, so this is easy-ish.
   *
   * @return HTTP/1.0 or HTTP/1.1. Or anything else that grammar::httpVersion
   * accepts.
   */
  std::string protocol(void) const { return std::string(version); }

  /* Status code description.
   *
   * Should pretty much be ignored, and is only useful for humans reading a
   * stream transcript. And even then it can't really be trusted.
   */
  std::string description;

  /* Create status line.
   *
   * Uses whatever data we have to create a status line. In case the input would
   * not be valid, a generic status line is created.
   *
   * @return Returns the status line in a form that could be used in HTTP.
   */
  operator std::string(void) const {
    if (!valid()) {
      return "HTTP/1.1 500 Bad Status Line\r\n";
    }

    return protocol() + " " + std::to_string(code) + " " + description + "\r\n";
  }

  /* Get status code text description.
   * @status The HTTP status code to look up.
   *
   * This is a separate function, because we need to provide a default for the
   * map lookup.
   *
   * @return A text description, which can be sent with your HTTP request.
   */
  static std::string getDescription(unsigned status) {
    auto it = http::status.find(status);
    if (it != http::status.end()) {
      return it->second;
    }

    return "Other Status";
  }
};
}  // namespace http
}  // namespace cxxhttp

#endif

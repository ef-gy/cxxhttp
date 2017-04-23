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

#include <optional>
#include <regex>
#include <string>

#include <cxxhttp/http-constants.h>

namespace cxxhttp {
namespace http {
/* Get status code text description.
 * @status The HTTP status code to look up.
 *
 * This is a separate function, because we need to provide a default for the
 * map lookup.
 *
 * @return A text description, which can be sent with your HTTP request.
 */
static std::string statusDescription(unsigned status) {
  auto it = http::status.find(status);
  if (it != http::status.end()) {
    return it->second;
  }

  return "Other Status";
}

/* Broken out status line.
 *
 * Contains all the data in a status line, broken out into the relevant fields.
 */
struct statusLine {
  /* The status code.
   *
   * Anything not in the http::status map is probably an error.
   */
  unsigned code;

  /* Protocol name.
   *
   * HTTP/1.0 or HTTP/1.1.
   */
  std::string protocol;

  /* Status code description.
   *
   * Should pretty much be ignored, and is only useful for humans reading a
   * stream transcript. And even then it can't really be trusted.
   */
  std::string description;
};

/* Parse HTTP status line.
 * @line The (suspected) status line to parse.
 *
 * This currently only accepts HTTP/1.0 and HTTP/1.1 status lines, all others
 * will be rejected.
 *
 * @return An empty optional, or the parsed status line.
 */
static std::optional<statusLine> parse(const std::string &line) {
  static const std::regex stat("(HTTP/1.[01])\\s+([0-9]{3})\\s+(.*)\\s*");
  std::smatch matches;

  if (std::regex_match(line, matches, stat)) {
    const std::string protocol = matches[1];
    const std::string description = matches[3];
    unsigned code = 500;
    try {
      code = std::stoi(matches[2]);
    } catch (...) {
    }
    return statusLine{code, protocol, description};
  } else {
    return std::optional<statusLine>();
  }
}
}
}

#endif

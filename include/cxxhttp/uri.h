/* URI handling code.
 *
 * All the code needed to handle URIs, based on RFC 3986.
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
#if !defined(CXXHTTP_URI_H)
#define CXXHTTP_URI_H

#include <map>
#include <regex>
#include <string>

namespace cxxhttp {
/* URI components.
 *
 * All the portions making up a URI.
 */
struct uriComponents {
  /* The URI's scheme.
   *
   * The scheme of a URI.
   */
  std::string scheme;

  /* The URI's authority.
   *
   * A URI's authority, like the host name in HTTP.
   */
  std::string authority;

  /* The URI's path.
   *
   * Everything past the host name, and before the query portion.
   */
  std::string path;

  /* The URI's query string.
   *
   * Everything after the initial '?', which is not part of the location portion
   * of a URI.
   */
  std::string query;

  /* A fragment identifier in a URI.
   *
   * Not actually used in the HTTP protocol, but might as well have it.
   */
  std::string fragment;
};

/* URI parser.
 *
 * Can take a URI and turn it into the relevant subcomponents, parsing and
 * decoding the pieces into the relevant parts.
 */
class uri {
 public:
  /* Default constructor.
   *
   * Initialise an empty URI. This defaults to being invalid.
   */
  uri(void) : isValid(false) {}

  /* Parse a given URI.
   * @pURI What to parse.
   *
   * Parse a URI by applying the regular expression in RFC 3986, appendix B.
   */
  uri(const std::string &pURI) {
    static const std::regex rx(
        "^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\\?([^#]*))?(#(.*))?");
    std::smatch match;

    isValid = std::regex_match(pURI, match, rx);

    if (isValid) {
      original = {
          match[2], match[4], match[5], match[7], match[9],
      };
    }

    decoded = {
        decode(original.scheme, isValid),   decode(original.authority, isValid),
        decode(original.path, isValid),     decode(original.query, isValid),
        decode(original.fragment, isValid),
    };
  }

  /* Report whether the URI is currently valid.
   *
   * Currently just a read-only way to access the isValid member variable.
   *
   * @return `isValid`.
   */
  bool valid(void) const { return isValid; }

  /* Get scheme.
   *
   * Decodes the scheme and returns it.
   *
   * @return The scheme, after being decoded.
   */
  std::string scheme(void) const { return decoded.scheme; }

  /* Get authority.
   *
   * Decodes the authority and returns it.
   *
   * @return The authority, after being decoded.
   */
  std::string authority(void) const { return decoded.authority; }

  /* Get path.
   *
   * Decodes the path and returns it.
   *
   * @return The path, after being decoded.
   */
  std::string path(void) const { return decoded.path; }

  /* Get query string.
   *
   * Decodes the query string and returns it.
   *
   * @return The query string, after being decoded.
   */
  std::string query(void) const { return decoded.query; }

  /* Get fragment.
   *
   * Decodes the fragment and returns it.
   *
   * @return The fragment, after being decoded.
   */
  std::string fragment(void) const { return decoded.fragment; }

  /* Decode a URI component.
   * @s The URI component to process.
   * @isValid Set to false iff decoding any part of the component failed.
   *
   * Resolves percent-encoded portions of the string.
   *
   * @return The decoded version of the component.
   */
  static std::string decode(const std::string &s, bool &isValid) {
    std::string rv;

    int expectEncoded = 0;
    int first = 0;

    for (const auto &c : s) {
      if (expectEncoded == 2) {
        expectEncoded = 0;
        rv.push_back(first | decode(c, isValid));
      } else if (expectEncoded == 1) {
        expectEncoded = 2;
        first = decode(c, isValid) << 4;
      } else if (c == '%') {
        expectEncoded = 1;
      } else {
        rv.push_back(c);
      }
    }

    isValid = isValid && (expectEncoded == 0);

    return rv;
  }

  /* Decode a form map.
   * @s A map with encoded form data.
   * @isValid Set to false iff decoding failed.
   *
   * Decodes a URI data map. As in, a application/x-www-form-urlencoded encoded
   * string.
   *
   * @return The decoded map.
   */
  static std::map<std::string, std::string> map(const std::string &s,
                                                bool &isValid) {
    std::map<std::string, std::string> rv;

    bool isKey = true;

    std::string key = "";
    std::string value = "";

    isValid = true;

    for (const auto &c : s) {
      if (isKey && c == '=') {
        isKey = false;
        value.clear();
      } else if (isKey) {
        key.push_back(c);
      } else if (c == '&') {
        isKey = true;
        rv[key] = decode(value, isValid);
        key.clear();
      } else {
        value.push_back(c);
      }
    }

    rv[key] = decode(value, isValid);
    isValid = isValid && !isKey;

    return rv;
  }

  /* Reconstitute URI.
   *
   * This creates a standard URI that can be sent on the wire and should parse
   * again afterwards.
   *
   * @return A URI, after being reconstructed from the individual parts.
   */
  operator std::string(void) const {
    return (original.scheme.empty() ? original.scheme : original.scheme + ":") +
           (original.authority.empty() ? original.authority
                                       : "//" + original.authority) +
           original.path +
           (original.query.empty() ? original.query : "?" + original.query) +
           (original.fragment.empty() ? original.fragment
                                      : "#" + original.fragment);
  }

 protected:
  /* Whether the URL parsing succeded.
   *
   * Set to `false` if parsing the URL failed in the constructor. If so, most
   * members are probably not containing anything useful.
   */
  bool isValid = true;

  /* Original URI components.
   *
   * Unmodified URI components, as used in the constructor.
   */
  uriComponents original;

  /* Decoded URI components.
   *
   * URI components, after decoding them.
   */
  uriComponents decoded;

  /* Decode hex digit.
   * @c The character to decode.
   * @isValid Set to false iff the input is not a valid hex digit.
   *
   * Used while parsing a URI, as percent-encoded portions are just using the
   * character's byte value in hex.
   *
   * @return The number represented by the hex character.
   */
  static int decode(int c, bool &isValid) {
    if (c >= '0' && c <= '9') {
      return c - '0';
    } else if (c >= 'a' && c <= 'f') {
      return 10 + (c - 'a');
    } else if (c >= 'A' && c <= 'F') {
      return 10 + (c - 'A');
    } else {
      isValid = false;
      return 0;
    }
  }
};
}  // namespace cxxhttp

#endif

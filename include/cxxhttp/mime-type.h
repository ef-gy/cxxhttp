/* MIME type handling code.
 *
 * The code in this file is based on RFCs 2045 and 2046, and deals with parsing
 * and comparing MIME media types.
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
#if !defined(CXXHTTP_MIME_TYPE_H)
#define CXXHTTP_MIME_TYPE_H

#include <map>
#include <regex>

#include <cxxhttp/string.h>

namespace cxxhttp {
/* Represents a MIME type.
 *
 * MIME types are specified in RFC 2045, section 5.1, which is the definition
 * of the Content-Type header. The media type is the value of the
 * Content-Type header in MIME, but is used in a number of other places as well.
 */
class mimeType {
 public:
  /* Attribute map type.
   *
   * MIME type attribute names are case-insensitive, so we need a special map
   * to deal with that.
   */
  using attributeMap = std::map<std::string, std::string, caseInsensitiveLT>;

  /* MIME type category.
   *
   * One of the handful of MIME type categories, e.g. text, audio, video, image
   * or application, or one of special types. The syntax would allow others as
   * well, but those are not necessarily defined.
   */
  std::string type;

  /* MIME subtype.
   *
   * Whereas <type> has the general category of the media type, this is the more
   * specific piece of information needed to make sense of the contents of a
   * corresponding file.
   */
  std::string subtype;

  /* MIME type attributes.
   *
   * Contains the key=value map elements used as parameters for MIME types. The
   * key in these is case-insensitive.
   */
  attributeMap attributes;

  /* Is this type valid?
   *
   * Set to false when trying to parse an invalid media type. This function also
   * performs some basic tests on the MIME type as defined in RFC 2046.
   *
   * @return Whether not the MIME type is currently valid as set.
   */
  bool valid(void) const { return isValid; }

  /* Parse MIME type.
   * @pType The string serialisation of a MIME type to parse.
   */
  mimeType(const std::string &pType) : isValid(true) {
    enum {
      inType,
      inSub,
      inKey,
      inValue,
      inValueQuoted,
      inValueEscaped,
    } state = inType;

    std::string key, value;
    bool space = false;

    for (const auto &c : pType) {
      if (!isValid) {
        // do nothing if we're in a bad state.
      } else if (state == inValueEscaped) {
        state = inValueQuoted;
        value.push_back(c);
      } else if (state == inValueQuoted && c == '"') {
        state = inValue;
      } else if (state == inValue && c == '"' && value.empty()) {
        state = inValueQuoted;
      } else if (state == inValueQuoted && c == '\\') {
        state = inValueEscaped;
      } else if (state == inValueQuoted) {
        value.push_back(c);
      } else if (state == inType && isToken(c) && (!space || type.empty())) {
        type.push_back(std::tolower(c));
      } else if (state == inSub && isToken(c) && (!space || subtype.empty())) {
        subtype.push_back(std::tolower(c));
      } else if (state == inKey && isToken(c) && (!space || key.empty())) {
        key.push_back(std::tolower(c));
      } else if (state == inValue && isToken(c) && (!space || value.empty())) {
        value.push_back(c);
      } else if (state == inType && c == '/' && !type.empty()) {
        state = inSub;
      } else if (state == inSub && c == ';' && !subtype.empty()) {
        state = inKey;
      } else if (state == inValue && c == ';') {
        isValid = !key.empty() && !value.empty();
        state = inKey;
        attributes[key] = value;
        key.clear();
        value.clear();
      } else if (state == inKey && c == '=') {
        state = inValue;
      } else if (!isSpace(c)) {
        // ignore free spaces; this is somewhat more lenient than the original
        // grammar, but then that also insists on some level of leniency there.
        isValid = false;
      }
      space = isSpace(c);
    }

    isValid = isValid && (state == inSub || state == inValue) &&
              (type != "*" || subtype == "*");

    if (isValid && state == inValue) {
      attributes[key] = value;
    }
  }

  /* Normalise string.
   *
   * Assuming a successful parse, this will turn the parsed MIME type into a
   * normalised form.
   *
   * @return A string, representing the parsed type.
   */
  operator std::string(void) const {
    if (!valid()) {
      return "invalid";
    }

    std::string rv = type + "/" + subtype;
    for (const auto &a : attributes) {
      std::string value;
      bool quotes = false;
      for (const auto &v : a.second) {
        if (!isToken(v)) {
          quotes = true;
          if (isCTL(v) || v == '"' || v == '\\') {
            value.push_back('\\');
          }
        }
        value.push_back(v);
      }
      rv += "; " + a.first + "=" + (quotes ? "\"" + value + "\"" : value);
    }

    return rv;
  }

 protected:
  /* Is this type valid?
   *
   * Set to false when trying to parse an invalid media type.
   */
  bool isValid;

  /* Check character against the set of control characters.
   * @c The character to check.
   *
   * The set of control characters is defined in RFC 822, section 3.3.
   *
   * @return Reports whether the character is a control character.
   */
  static bool isCTL(int c) { return c <= 31 || c == 127; }

  /* Check character against the set of special characters.
   * @c The character to check.
   *
   * Defined in RFC 2045, section 5.1.
   *
   * If any of these characters occurs in a parameter value, that value needs to
   * be in quotes.
   *
   * @return Reports whether the character is a special character.
   */
  static bool isTSpecial(int c) {
    switch (c) {
      case '(':
      case ')':
      case '<':
      case '>':
      case '@':
      case ',':
      case ';':
      case ':':
      case '\\':
      case '"':
      case '/':
      case '[':
      case ']':
      case '?':
      case '=':
        return true;
      default:
        return false;
    }
  }

  /* Check if character can be represented with 7 bits.
   * @c The character to check.
   *
   * MIME is a 7-bit protocol at heart, so the header fields, and by extension
   * the MIME types need to be 7-bit safe.
   *
   * @return Reports whether the character is a valid 7-bit ASCII character.
   */
  static bool is7Bit(int c) { return c >= 0 && c <= 127; }

  /* Check whether the character is a valid token character.
   * @c The character to check.
   *
   * Token characters are used for most parts of a MIME type, and are defined in
   * RFC 2045, section 5.1.
   *
   * Original grammar rule:
   *     token := 1*<any (US-ASCII) CHAR except SPACE, CTLs,
   *                 or tspecials>
   *
   * @return Reports whether the character is a valid character in a token.
   */
  static bool isToken(int c) {
    return is7Bit(c) && c != ' ' && !isCTL(c) && !isTSpecial(c);
  }

  /* Check whether the character is linear white space.
   * @c The character to check.
   *
   * Linear white space for the purposes of parsing things contained in header
   * fields is basically spaces and tab characters.
   *
   * @return Reports whether the character is linear white space.
   */
  static bool isSpace(int c) { return c == ' ' || c == '\t'; }
};
}

#endif

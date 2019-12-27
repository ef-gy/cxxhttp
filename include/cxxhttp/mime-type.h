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

#include <cxxhttp/string.h>

#include <map>
#include <regex>

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

  /* Default Constructor.
   *
   * Produces a MIME type instance that is tagged as invalid.
   */
  mimeType() : isValid(false) {}

  /* Parse MIME type.
   * @pType The string serialisation of a MIME type to parse.
   *
   * Does a proper parse of the given media type. The <isValid> member will be
   * set afterwards to indicate whether the input string was a valid MIME type.
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
    std::string *collectTo = &type;
    bool space = false;

    for (const auto &c : pType) {
      bool token = isToken(c);
      bool tokenShiftSpace = token && !space;
      bool tokenTargetEmpty = token && collectTo->empty();
      bool doCollect = tokenShiftSpace || tokenTargetEmpty;
      bool pushLC =
          doCollect && (state == inType || state == inSub || state == inKey);
      bool pushCS = state == inValueQuoted || (doCollect && state == inValue);
      space = isSpace(c);

      if (state == inValueEscaped) {
        state = inValueQuoted;
        collectTo->push_back(c);
      } else if (state == inValueQuoted && c == '"') {
        state = inValue;
      } else if (state == inValueQuoted && c == '\\') {
        state = inValueEscaped;
      } else if (state == inValue && c == '"' && value.empty()) {
        state = inValueQuoted;
      } else if (state == inType && c == '/' && !type.empty()) {
        state = inSub;
        collectTo = &subtype;
      } else if (state == inSub && c == ';' && !subtype.empty()) {
        state = inKey;
        collectTo = &key;
      } else if (state == inValue && c == ';') {
        attributes[key] = value;
        state = inKey;
        key.clear();
        value.clear();
        collectTo = &key;
      } else if (state == inKey && c == '=') {
        state = inValue;
        collectTo = &value;
        isValid = isValid && !key.empty();
      } else if (pushLC) {
        collectTo->push_back(std::tolower(c));
      } else if (pushCS) {
        collectTo->push_back(c);
      } else if (!space) {
        // ignore free spaces; this is somewhat more lenient than the original
        // grammar, but then that also insists on some level of leniency there.
        isValid = false;
      }

      if (state == inSub && type == "*" && !subtype.empty()) {
        isValid = subtype == "*";
      }

      if (!isValid) {
        break;
      }
    }

    if (state == inValue) {
      attributes[key] = value;
    } else if (state != inSub) {
      isValid = false;
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

  /* Less-than comparator.
   * @b Right-hand side of the comparison.
   *
   * Compares the value in `b` to this value, and returns `true` if this value
   * is considered to come before the `b` value in a lexical sort.
   *
   * This is implemented using the normalised string form of the two types in
   * question.
   *
   * @return Whether this value is less than `b`.
   */
  bool operator<(const mimeType &b) const {
    return std::string(*this) < std::string(b);
  }

  /* Equality operator.
   * @b The instance to compare to.
   *
   * Matching is performed based on the full attribute values. Full wildcards
   * for either the type or subtype are allowed as well.
   *
   * Two types do not match if both sides have wildcards.
   *
   * @return Whether or not the two mime types match.
   */
  bool operator==(const mimeType &b) const {
    return typeMatch(b) && attributes == b.attributes;
  }

  /* Subset operator.
   * @b The instance to compare to.
   *
   * Similar to the quality operator, except that the attribute set is allowed
   * to be a subset of the right-hand side.
   * This is for cases where two mime types are basically the same, except one
   * side is a more specific version of the other side, e.g. the charset is
   * specified explicitly.
   *
   * @return Whether or not the two mime types match.
   */
  bool operator<=(const mimeType &b) const {
    if (!typeMatch(b)) {
      return false;
    }

    // we already have a type match at this point, so just check that all
    // attributes in this instance are the same in the b instance.
    for (const auto &v : attributes) {
      const auto &bv = b.attributes.find(v.first);
      if (bv == b.attributes.end() || bv->second != v.second) {
        return false;
      }
    }

    return true;
  }

  /* Does this media type contain wildcards?
   *
   * `haveWildcards()` checks if either the type or the subtype is a literal
   * asterisk character, which is normally used to denote wildcards for media
   * type matching.
   *
   * @return Whether either of the components is a wildcard character.
   */
  bool wildcard(void) const {
    return valid() && (type == "*" || subtype == "*");
  }

 protected:
  /* Is this type valid?
   *
   * Set to false when trying to parse an invalid media type.
   */
  bool isValid;

  /* Type equality match.
   * @b The instance to compare to.
   *
   * Full wildcards for either the type or subtype are allowed as well.
   * Two types do not match if both sides have wildcards.
   * Does not look at attribute values.
   *
   * @return Whether or not the two mime types match.
   */
  bool typeMatch(const mimeType &b) const {
    return valid() && b.valid() && (!wildcard() || !b.wildcard()) &&
           (type == "*" || b.type == "*" || type == b.type) &&
           (subtype == "*" || b.subtype == "*" || subtype == b.subtype);
  }

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
   *     token := 1*(any (US-ASCII) CHAR except SPACE, CTLs,
   *                 or tspecials)
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
}  // namespace cxxhttp

#endif

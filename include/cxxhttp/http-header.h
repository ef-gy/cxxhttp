/* asio.hpp HTTP header type.
 *
 * The headers data type is a basic map, but with a case-insensitive comparator,
 * which is necessary for processing HTTP/1.1 headers as this makes keys that
 * only differ in their case work as expected.
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
#if !defined(CXXHTTP_HTTP_HEADER_H)
#define CXXHTTP_HTTP_HEADER_H

#include <locale>
#include <map>
#include <regex>
#include <set>
#include <string>

#include <cxxhttp/http-grammar.h>

namespace cxxhttp {
namespace http {
/* Case-insensitive comparison functor
 *
 * A simple functor used by the attribute map to compare strings without
 * caring for the letter case.
 */
class headerNameLT
    : private std::binary_function<std::string, std::string, bool> {
 public:
  /* Case-insensitive string comparison
   * @a The first of the two strings to compare.
   * @b The second of the two strings to compare.
   *
   * Compares two strings case-insensitively.
   *
   * @return 'true' if the first string is "less than" the second.
   */
  bool operator()(const std::string &a, const std::string &b) const {
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
                                        compare);
  }

 protected:
  /* Lower-case comparison function.
   * @c1 The left-hand side of the comparison.
   * @c2 The right-hand side of the comparison.
   *
   * Used by the operator() as the comparator for std::lexicographical_compare.
   *
   * @return true, if c1 comes before c2 after converting both to lower-case.
   */
  static bool compare(unsigned char c1, unsigned char c2) {
    return std::tolower(c1) < std::tolower(c2);
  }
};

/* HTTP header type.
 *
 * This is a basic string-to-string map, but with the case-insensitive
 * comparison operator from above. Usage is the same as any other map, except
 * that keys are effectively considered the same if they only differ in their
 * case.
 */
using headers = std::map<std::string, std::string, headerNameLT>;

template <class headers>
class parser {
 public:
  /* Header map.
   *
   * Populated with absorb() and append().
   */
  headers header;

  /* Name of the last parsed header
   *
   * This is the name of the last header line that was parsed. Used with
   * multi-line headers.
   */
  std::string lastHeader;

  /* Append value to header map.
   * @key The key to append to, or set.
   * @value The new value.
   *
   * Appends 'value' to the element 'key' in the header map. The former and the
   * new value will be separated by a ',', which is used throughout HTTP/1.1
   * header fields for when lists need to be represented.
   *
   * If the key was not originally set, then the value is simply set instead of
   * appended. This keeps the HTTP/1.1 header list syntax happy.
   *
   * @return 'true' if the value was appended, 'false' otherwise, i.e. if the
   * value was set instead.
   */
  bool append(const std::string &key, const std::string &value) {
    if (!value.empty()) {
      std::string &v = header[key];
      if (v.empty()) {
        v = value;
        return false;
      }

      v += "," + value;
    }
    return true;
  }

  /* Parse and append header line.
   * @line The raw line to parse and absorb.
   *
   * Parse a header line using MIME header rules, and append any keys or values
   * to the header instance.
   *
   * @return 'true' if the line was successfully parsed as a header.
   */
  bool absorb(const std::string &line) {
    static const std::string captureName = "(" + grammar::fieldName + ")";
    static const std::string captureValue = "(" + grammar::fieldContent + ")?";

    static const std::regex headerProper(captureName + ":" + grammar::ows +
                                         captureValue + grammar::ows +
                                         "\r?\n?");
    static const std::regex headerContinued(grammar::rws + captureValue +
                                            grammar::ows + "\r?\n?");

    std::smatch matches;

    bool matched =
        !lastHeader.empty() && std::regex_match(line, matches, headerContinued);
    std::string appendValue;

    if (matched) {
      appendValue = matches[1];
    } else {
      matched = std::regex_match(line, matches, headerProper);

      if (matched) {
        lastHeader = matches[1];

        // RFC 2616, section 4.2:
        // Header fields that occur multiple times must be combinable into a
        // single
        // value by appending the fields in the order they occur, using commas
        // to
        // separate the individual values.
        appendValue = matches[2];
      }
    }

    if (!appendValue.empty()) {
      append(lastHeader, appendValue);
    }

    return matched;
  }

  /* Insert a different header map.
   * @map The map to merge in.
   *
   * Merges headers with the given new map. This uses std::map::insert(), which
   * will not overwrite values that already but instead only insert elements
   * that don't exist yet.
   */
  void insert(const headers &map) { header.insert(map.begin(), map.end()); }

  /* Turn a header map into a string.
   * @header The header map to turn into a string.
   *
   * This function takes a header map and converts it into the form used in the
   * HTTP protocol. This form is, essentially, "Key: Value<CR><LN>".
   *
   * @return A string, with all of the elements in the header parameter.
   */
  operator std::string(void) const {
    std::string r = "";
    for (const auto &h : header) {
      r += h.first + ": " + h.second + "\r\n";
    }
    return r;
  }
};
}
}

#endif

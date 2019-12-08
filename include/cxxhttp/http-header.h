/* HTTP header type.
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

#include <cxxhttp/http-grammar.h>
#include <cxxhttp/string.h>

#include <map>
#include <regex>
#include <set>

namespace cxxhttp {
namespace http {
/* HTTP header type.
 *
 * This is a basic string-to-string map, but with the case-insensitive
 * comparison operator. Usage is the same as any other map, except that keys are
 * effectively considered the same if they only differ in their case.
 */
using headers = std::map<std::string, std::string, caseInsensitiveLT>;

/* Header parser functionality.
 * @headers The map-like type to use when parsing headers.
 *
 * Contains the state needed to parse a header block, and to determine what the
 * current status of that is.
 */
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

  /* Whether headers have been read in completely.
   *
   * Set in the absorb() method, based on whether there was a final line or not.
   */
  bool complete;

  /* Get header a value, possibly substituting a default.
   * @name The name of the header to fetch.
   * @def The default, if the header was not set.
   *
   * `get()` retrieves a header value and returns it; if the header wasn't set
   * then the `def` value is returned instead.
   *
   * @return The header, or the default.
   */
  std::string get(const std::string &name, const std::string &def = "") const {
    const auto &it = header.find(name);
    return it == header.end() ? def : it->second;
  }

  /* Append value to header map.
   * @key The key to append to, or set.
   * @value The new value.
   * @lws Append with white-space instead of a comma.
   *
   * Appends 'value' to the element 'key' in the header map. The former and the
   * new value will be separated by a ',', which is used throughout HTTP/1.1
   * header fields for when lists need to be represented, unless the lws flag is
   * set, in which case separator is a single space.
   *
   * If the key was not originally set, then the value is simply set instead of
   * appended. This keeps the HTTP/1.1 header list syntax happy.
   */
  void append(const std::string &key, const std::string &value,
              bool lws = false) {
    if (!value.empty()) {
      std::string &v = header[key];
      v += (v.empty() ? "" : (lws ? " " : ",")) + value;
    }
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
    static const std::regex finalLine("\r?\n?");

    bool matched = std::regex_match(line, finalLine);
    complete = matched;

    if (!complete) {
      std::smatch matches;

      matched = !lastHeader.empty() &&
                std::regex_match(line, matches, headerContinued);
      std::string appendValue;
      bool lws = matched;

      if (matched) {
        appendValue = matches[1];
      } else {
        matched = std::regex_match(line, matches, headerProper);

        if (matched) {
          lastHeader = matches[1];

          // RFC 2616, section 4.2:
          // Header fields that occur multiple times must be combinable into a
          // single value by appending the fields in the order they occur, using
          // commas to separate the individual values.
          appendValue = matches[2];
        }
      }

      append(lastHeader, appendValue, lws);
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
   *
   * This function takes a header map and converts it into the form used in the
   * HTTP protocol. This form is, essentially, "Key: ValueCRLN".
   *
   * @return A string, with all of the elements in the header parameter.
   */
  operator std::string(void) const {
    std::string r;
    for (const auto &h : header) {
      r += h.first + ": " + h.second + "\r\n";
    }
    return r;
  }
};
}  // namespace http
}  // namespace cxxhttp

#endif

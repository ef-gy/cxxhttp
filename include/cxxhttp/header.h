/**\file
 * \brief HTTP header type.
 *
 * The headers data type is a basic map, but with a case-insensitive comparator,
 * which is necessary for processing HTTP/1.1 headers as this makes keys that
 * only differ in their case work as expected.
 *
 * \copyright
 * This file is part of the cxxhttp project, which is released as open source
 * under the terms of an MIT/X11-style licence, described in the COPYING file.
 *
 * \see Project Documentation: https://ef.gy/documentation/cxxhttp
 * \see Project Source Code: https://github.com/ef-gy/cxxhttp
 * \see Licence Terms: https://github.com/ef-gy/cxxhttp/blob/master/COPYING
 */

#if !defined(CXXHTTP_HEADER_H)
#define CXXHTTP_HEADER_H

#include <map>
#include <locale>
#include <set>
#include <string>

namespace cxxhttp {
namespace comparator {
/**\brief Case-insensitive comparison functor
 *
 * A simple functor used by the attribute map to compare strings without
 * caring for the letter case.
 */
class headerNameLT
    : private std::binary_function<std::string, std::string, bool> {
 public:
  /**\brief Case-insensitive string comparison
   *
   * Compares two strings case-insensitively.
   *
   * \param[in] a The first of the two strings to compare.
   * \param[in] b The second of the two strings to compare.
   *
   * \returns 'true' if the first string is "less than" the second.
   */
  bool operator()(const std::string &a, const std::string &b) const {
    return std::lexicographical_compare(
        a.begin(), a.end(), b.begin(), b.end(), compare);
  }

 protected:
  /**\brief Lower-case comparison function.
   *
   * Used by the operator() as the comparator for std::lexicographical_compare.
   *
   * \param[in] c1 The left-hand side of the comparison.
   * \param[in] c2 The right-hand side of the comparison.
   * \returns true, if c1 comes before c2 after converting both to lower-case.
   */
  static bool compare(unsigned char c1, unsigned char c2) {
    return std::tolower(c1) < std::tolower(c2);
  }
};
}

/**\brief HTTP header type.
 *
 * This is a basic string-to-string map, but with the case-insensitive
 * comparison operator from above. Usage is the same as any other map, except
 * that keys are effectively considered the same if they only differ in their
 * case.
 */
using headers = std::map<std::string, std::string, comparator::headerNameLT>;

/**\brief Turn a header map into a string.
 *
 * This function takes a header map and converts it into the form used in the
 * HTTP protocol. This form is, essentially, "Key: Value<CR><LN>".
 *
 * \param[in] header The header map to turn into a string.
 *
 * \returns A string, with all of the elements in the header parameter.
 */
static inline std::string to_string(const headers &header) {
  std::string r = "";
  for (const auto &h : header) {
    r += h.first + ": " + h.second + "\r\n";
  }
  return r;
}
}

#endif

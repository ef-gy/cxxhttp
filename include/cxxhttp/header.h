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

#include <locale>
#include <map>
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
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
                                        compare);
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
 * \tparam comp A comparator for the map.
 * \param[in] header The header map to turn into a string.
 * \returns A string, with all of the elements in the header parameter.
 */
template <typename comp>
static inline std::string to_string(
    const std::map<std::string, std::string, comp> &header) {
  std::string r = "";
  for (const auto &h : header) {
    r += h.first + ": " + h.second + "\r\n";
  }
  return r;
}

/**\brief Append value to header map.
 *
 * Appends 'value' to the element 'key' in the header map. The former and the
 * new value will be separated by a ',', which is used throughout HTTP/1.1
 * header fields for when lists need to be represented.
 *
 * If the key was not originally set, then the value is simply set instead of
 * appended. This keeps the HTTP/1.1 header list syntax happy.
 *
 * \tparam comp A comparator for the map.
 * \param[in,out] header The map to modify.
 * \param[in]     key    The key to append to, or set.
 * \param[in]     value  The new value.
 * \returns 'true' if the value was appended, 'false' otherwise, i.e. if the
 *     value was set instead.
 */
template <typename comp>
static inline bool append(std::map<std::string, std::string, comp> &header,
                          const std::string &key, const std::string &value) {
  std::string &v = header[key];
  if (v.empty()) {
    v = value;
    return false;
  }

  v += "," + value;
  return true;
}
}

#endif

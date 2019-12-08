/* String manipulation.
 *
 * Common string manipulation functions and classes. There's a theme to how
 * string parsing works across IETF RFCs, and this header contains portions that
 * are used in discrete portions of code.
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
#if !defined(CXXHTTP_STRING_H)
#define CXXHTTP_STRING_H

#include <locale>
#include <string>

namespace cxxhttp {
/* Case-insensitive comparison functor
 *
 * A simple functor used by the attribute map to compare strings without
 * caring for the letter case.
 */
class caseInsensitiveLT
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
}  // namespace cxxhttp

#endif

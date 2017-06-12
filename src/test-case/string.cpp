/* Test cases for the string manipulation code.
 *
 * Contains test cases for cxxhttp/string.h, which implements some shared
 * algorithms that are used by other header files.
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

#include <ef.gy/test-case.h>

#include <cxxhttp/string.h>

using namespace cxxhttp;

/* Test case-insensitive comparator.
 * @log Test output stream.
 *
 * HTTP requires headers to be case-insensitive, so this test makes sure that
 * the function we implemented for that works as intended. This is a table based
 * test where the test data is used for the comparison both back and forward.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testCompare(std::ostream &log) {
  struct sampleData {
    std::string a, b;
    bool res, rev;
  };

  std::vector<sampleData> tests{
      {"a", "b", true, false},
      {"a", "a", false, false},
      {"a", "A", false, false},
      {"aa", "ab", true, false},
      {"aA", "Aa", false, false},
  };

  for (const auto &tt : tests) {
    const auto v = caseInsensitiveLT()(tt.a, tt.b);
    if (v != tt.res) {
      log << "caseInsensitiveLT('" << tt.a << "' < '" << tt.b << "')='" << v
          << "', expected '" << tt.res << "'\n";
      return false;
    }
    const auto v2 = caseInsensitiveLT()(tt.b, tt.a);
    if (v2 != tt.rev) {
      log << "caseInsensitiveLT('" << tt.b << "' < '" << tt.a << "')='" << v2
          << "', expected '" << tt.rev << "'\n";
      return false;
    }
  }

  return true;
}

namespace test {
using efgy::test::function;

static function compare(testCompare);
}

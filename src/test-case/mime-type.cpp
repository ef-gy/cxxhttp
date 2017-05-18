/* Test cases for the MIME type parser.
 *
 * MIME types are a bit finicky and require special parsing. This tests the
 * parser an some of the associated functions.
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

#include <cxxhttp/mime-type.h>

using namespace cxxhttp;

/* Test MIME type parser.
 * @log Test output stream.
 *
 * Parses a few sample MIME types and compares the result with what it should
 * look like.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testParser(std::ostream &log) {
  struct sampleData {
    std::string in, type, subtype;
    mimeType::attributeMap attributes;
    bool valid;
  };

  std::vector<sampleData> tests{
      {"foo/bar", "foo", "bar", {}, true},
      {"foo/bar ", "foo", "bar", {}, true},
      {"FoO/BaR ", "foo", "bar", {}, true},
      {"FoO/*", "foo", "*", {}, true},
      {"*/*", "*", "*", {}, true},
      {"*/bar", "", "", {}, false},
      {"foo/bar; ", "", "", {}, false},
      {"foo/bar ;", "", "", {}, false},
      {" foo / bar ", "foo", "bar", {}, true},
      {"fo o/bar", "", "", {}, false},
      {"foo/b ar", "", "", {}, false},
      {"foo/bar; a b=c", "", "", {}, false},
      {"foo/bar;A=b", "foo", "bar", {{"a", "b"}}, true},
      {"foo/bar; a=\"b\"", "foo", "bar", {{"a", "b"}}, true},
      {"foo/bar; a=\"b\" ", "foo", "bar", {{"a", "b"}}, true},
      {"foo/bar ; a= b ; c = d", "foo", "bar", {{"a", "b"}, {"c", "d"}}, true},
      {"foo/bar ; a=b ; c = \" d\" ",
       "foo",
       "bar",
       {{"a", "b"}, {"c", " d"}},
       true},
      {"foo/bar ; a =b ;c = \" d\"\" ", "", "", {}, false},
      {"foo/bar ; a =b ;c = \" d\\\"\" ",
       "foo",
       "bar",
       {{"a", "b"}, {"c", " d\""}},
       true},
  };

  for (const auto &tt : tests) {
    const mimeType v = tt.in;
    if (v.valid() != tt.valid) {
      log << "mimeType('" << tt.in << "').valid='" << v.valid()
          << "', expected '" << tt.valid << "'\n";
      return false;
    }
    if (v.valid()) {
      if (v.type != tt.type) {
        log << "mimeType('" << tt.in << "').type='" << v.type << "', expected '"
            << tt.type << "'\n";
        return false;
      }
      if (v.subtype != tt.subtype) {
        log << "mimeType('" << tt.in << "').subtype='" << v.subtype
            << "', expected '" << tt.subtype << "'\n";
        return false;
      }
      if (v.attributes != tt.attributes) {
        log << "mimeType('" << tt.in << "').attributes value is unexpected.\n";
        return false;
      }
    }
  }

  return true;
}

/* Test MIME type normalising.
 * @log Test output stream.
 *
 * Parses a few sample MIME types, and then normalises them. This ensures that
 * further comparisons work in a defined manner.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testNormalise(std::ostream &log) {
  struct sampleData {
    std::string in, out;
  };

  std::vector<sampleData> tests{
      {"fo o/ba r", "invalid"},
      {"foo/bar", "foo/bar"},
      {"FoO/BaR ;A =b", "foo/bar; a=b"},
      {"FoO/BaR ;A =\"b\"", "foo/bar; a=b"},
      {"FoO/BaR ;A =\"b \"", "foo/bar; a=\"b \""},
      {"FoO/BaR ;A =\"b\\\"\"", "foo/bar; a=\"b\\\"\""},
      {"FoO/BaR ;A =\"b\\.\"", "foo/bar; a=b."},
      {"FoO/BaR ;A =\"b\\ \"", "foo/bar; a=\"b \""},
      {"FoO/BaR ;A =\"b\\ \"; c=d", "foo/bar; a=\"b \"; c=d"},
      {"FoO/BaR ; c=f; A =\"b\\ \"", "foo/bar; a=\"b \"; c=f"},
  };

  for (const auto &tt : tests) {
    const mimeType v = tt.in;
    if (std::string(v) != tt.out) {
      log << "mimeType('" << tt.in << "').out='" << std::string(v)
          << "', expected '" << tt.out << "'\n";
      return false;
    }
  }

  return true;
}

namespace test {
using efgy::test::function;

static function parser(testParser);
static function normalise(testNormalise);
}

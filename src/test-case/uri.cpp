/* Test cases for URI handling.
 *
 * We use sample data to compare what the parser produced and what it should
 * have produced.
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

#include <cxxhttp/uri.h>

using namespace cxxhttp;

/* Test URI parsing.
 * @log Test output stream.
 *
 * Some test cases for parsing URIs into its individual components.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testParsing(std::ostream &log) {
  struct sampleData {
    std::string in;
    bool valid;
    std::string scheme, authority, path, query, fragment, out;
  };

  std::vector<sampleData> tests{
      {"http://ef.gy/", true, "http", "ef.gy", "/", "", "", "http://ef.gy/"},
      {"foo%20bar", true, "", "", "foo bar", "", "", "foo%20bar"},
      {"%frob", false, "", "", "", "", "", "%frob"},
      {"%2aob", true, "", "", "*ob", "", "", "%2aob"},
      {"%2Aob", true, "", "", "*ob", "", "", "%2Aob"},
      {"%2", false, "", "", "", "", "", "%2"},
      {"#foo", true, "", "", "", "", "foo", "#foo"},
  };

  for (const auto &tt : tests) {
    const auto v = uri(tt.in);

    if (v.valid() != tt.valid) {
      log << "uri('" << tt.in << "').valid() = " << v.valid() << "\n";
      return false;
    }

    if (v.valid()) {
      if (v.scheme() != tt.scheme) {
        log << "uri('" << tt.in << "').scheme = '" << v.scheme()
            << "', expected '" << tt.scheme << "'\n";
        return false;
      }
      if (v.authority() != tt.authority) {
        log << "uri('" << tt.in << "').authority = '" << v.authority()
            << "', expected '" << tt.authority << "'\n";
        return false;
      }
      if (v.path() != tt.path) {
        log << "uri('" << tt.in << "').path = '" << v.path() << "', expected '"
            << tt.path << "'\n";
        return false;
      }
      if (v.query() != tt.query) {
        log << "uri('" << tt.in << "').query = '" << v.query()
            << "', expected '" << tt.query << "'\n";
        return false;
      }
      if (v.fragment() != tt.fragment) {
        log << "uri('" << tt.in << "').fragment = '" << v.fragment()
            << "', expected '" << tt.fragment << "'\n";
        return false;
      }
      if (std::string(v) != tt.out) {
        log << "uri('" << tt.in << "') = '" << std::string(v) << "', expected '"
            << tt.out << "'\n";
        return false;
      }
    }
  }

  return true;
}

namespace test {
using efgy::test::function;

static function parsing(testParsing);
}

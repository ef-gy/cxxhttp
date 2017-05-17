/* Test cases for the headers data type.
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

#include <ef.gy/test-case.h>

#include <cxxhttp/http-header.h>

using namespace cxxhttp;
using namespace cxxhttp::http;

/* Test header flattening.
 * @log Test output stream.
 *
 * The header structure needs to be written out to a string, which represents a
 * MIME header form. Thhis is a table based test to do so.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testToString(std::ostream &log) {
  struct sampleData {
    headers in;
    std::string out;
  };

  std::vector<sampleData> tests{
      {{}, ""},
      {{{"a", "b"}}, "a: b\r\n"},
      {{{"a", "b"}, {"A", "c"}}, "a: b\r\n"},
      {{{"a", "b"}, {"c", "d"}}, "a: b\r\nc: d\r\n"},
  };

  for (const auto &tt : tests) {
    parser<headers> p{tt.in};
    const std::string v = p;
    if (v != tt.out) {
      log << "string()='" << v << "', expected '" << tt.out << "'\n";
      return false;
    }
  }

  return true;
}

/* Test header append function.
 * @log Test output stream.
 *
 * Appending a header in HTTP means either setting the header or adding a comma
 * and then the additional value. This test exercises that, and also exercises
 * whether the type is properly case insensitive.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testAppend(std::ostream &log) {
  struct sampleData {
    headers in;
    std::string key, value, out;
  };

  std::vector<sampleData> tests{
      {{}, "a", "b", "a: b\r\n"},
      {{{"a", "b"}}, "a", "c", "a: b,c\r\n"},
      {{{"a", "b"}, {"A", "c"}}, "A", "d", "a: b,d\r\n"},
      {{{"a", "b"}, {"c", "d"}}, "a", "e", "a: b,e\r\nc: d\r\n"},
  };

  for (const auto &tt : tests) {
    parser<headers> p{tt.in, ""};
    p.append(tt.key, tt.value);
    const std::string v = p;
    if (v != tt.out) {
      log << "string()='" << v << "', expected '" << tt.out << "'\n";
      return false;
    }
  }

  return true;
}

/* Test header line parsing.
 * @log Test output stream.
 *
 * This test appends single header lines, with various end-of-line characters,
 * to prepared headers and verifies that they parsed correctly by turning the
 * result into a flat header string.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testAbsorb(std::ostream &log) {
  struct sampleData {
    headers in;
    std::string line, last, out, res;
    bool match, complete;
  };

  std::vector<sampleData> tests{
      {{}, "a: b", "", "a: b\r\n", "a", true, false},
      {{}, "a: b\r\n", "b", "a: b\r\n", "a", true, false},
      {{{"a", "b"}}, "b:", "a", "a: b\r\n", "b", true, false},
      {{{"a", "b"}}, "a: c", "a", "a: b,c\r\n", "a", true, false},
      {{{"a", "b"}}, "\td\r\n", "a", "a: b d\r\n", "a", true, false},
      {{{"a", "b"}, {"c", "d"}},
       "a: e\r",
       "e",
       "a: b,e\r\nc: d\r\n",
       "a",
       true,
       false},
      {{}, "bad line", "", "", "", false, false},
      {{{"a", "b"}}, "", "a", "a: b\r\n", "a", true, true},
  };

  for (const auto &tt : tests) {
    parser<headers> p{tt.in, tt.last};
    const auto a = p.absorb(tt.line);
    const std::string v = p;
    if (v != tt.out) {
      log << "string()='" << v << "', expected '" << tt.out << "'\n";
      return false;
    }
    if (p.lastHeader != tt.res) {
      log << "absorb(" << tt.out << ") matched the wrong header: '"
          << p.lastHeader << "'\n";
      return false;
    }
    if (a != tt.match) {
      log << "absorb(" << tt.out
          << ") did not have the expected matching state\n";
      return false;
    }
    if (p.complete != tt.complete) {
      log << "absorb(" << tt.out
          << ") had the wrong completeness flag value.\n";
    }
  }

  return true;
}

/* Test if assigning an empty header parser clears all state.
 * @log Test output stream.
 *
 * Makes sure that assigning an empty header parser instance actually clears the
 * previous instance's state entirely. We rely on this in the code.
 *
 * As a side-effect, this is a quick syntax check to see that the compiler
 * supports proper list initialisations.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testClear(std::ostream &log) {
  parser<headers> p{{{"a", "b"}}, "c"};

  if (p.header.empty() || p.lastHeader.empty()) {
    return false;
  }

  p = {};

  if (!p.header.empty() || !p.lastHeader.empty()) {
    return false;
  }

  return true;
}

/* Test if inserting works as expected.
 * @log Test output stream.
 *
 * This merges two headers with 'insert' to see if the semantics of that are as
 * they should be.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testMerge(std::ostream &log) {
  struct sampleData {
    headers a, b, out;
  };

  std::vector<sampleData> tests{
      {{}, {}, {}},
      {{{"a", "b"}}, {{"c", "d"}}, {{"a", "b"}, {"c", "d"}}},
      {{{"a", "b"}}, {{"a", "d"}}, {{"a", "b"}}},
      {{{"A", "b"}}, {{"a", "d"}}, {{"A", "b"}}},
      {{{"a", "b"}}, {{"A", "e"}, {"c", "d"}}, {{"a", "b"}, {"c", "d"}}},
  };

  for (const auto &tt : tests) {
    parser<headers> p{tt.a};
    p.insert(tt.b);
    if (p.header != tt.out) {
      parser<headers> a{tt.a};
      parser<headers> b{tt.b};
      parser<headers> out{tt.out};
      log << "bad header merge; expected:\n"
          << std::string(out) << "got:\n"
          << std::string(p) << "with left hand side:\n"
          << std::string(a) << "and right hand side:\n"
          << std::string(b);
      return false;
    }
  }

  return true;
}

namespace test {
using efgy::test::function;

static function toString(testToString);
static function append(testAppend);
static function absorb(testAbsorb);
static function clear(testClear);
}

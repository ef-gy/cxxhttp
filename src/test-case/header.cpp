/**\file
 * \brief Test cases for the headers data type.
 *
 * The headers data type is a basic map, but with a case-insensitive comparator,
 * which is necessary for processing HTTP/1.1 headers as this makes keys that
 * only differ in their case work as expected.
 *
 * \copyright
 * This file is part of the cxxhttp project, which is released as open source
 * under the terms of an MIT/X11-style licence, described in the COPYING file.
 *
 * \see Project Documentation: http://ef.gy/documentation/cxxhttp
 * \see Project Source Code: https://github.com/ef-gy/cxxhttp
 * \see Licence Terms: https://github.com/ef-gy/cxxhttp/blob/master/COPYING
 */

#include <iostream>

#include <ef.gy/test-case.h>

#include <cxxhttp/header.h>

using namespace cxxhttp;

/**\brief Test header flattening.
 *
 * \test The header structure needs to be written out to a string, which
 *     represents a MIME header form. Thhis is a table based test to do so.
 *
 * \param[out] log
 * \returns Zero on success, nonzero otherwise.
 */
int testToString(std::ostream &log) {
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
    const auto v = to_string(tt.in);
    if (v != tt.out) {
      log << "to_string()='" << v << "', expected '" << tt.out << "'\n";
      return 1;
    }
  }

  return 0;
}

/**\brief Test case-insensitive comparator.
 *
 * \test HTTP requires headers to be case-insensitive, so this test makes sure
 *     that the function we implemented for that works as intended. This is a
 *     table based test where the test data is used for the comparison both back
 *     and forward.
 *
 * \param[out] log
 * \returns Zero on success, nonzero otherwise.
 */
int testCompare(std::ostream &log) {
  struct sampleData {
    std::string a, b;
    bool res, rev;
  };

  std::vector<sampleData> tests{
      {"a", "b", true, false},    {"a", "a", false, false},
      {"a", "A", false, false},   {"aa", "ab", true, false},
      {"aA", "Aa", false, false},
  };

  for (const auto &tt : tests) {
    const auto v = comparator::headerNameLT()(tt.a, tt.b);
    if (v != tt.res) {
      log << "headerNameLT('" << tt.a << "' < '" << tt.b << "')='" << v
          << "', expected '" << tt.res << "'\n";
      return 1;
    }
    const auto v2 = comparator::headerNameLT()(tt.b, tt.a);
    if (v2 != tt.rev) {
      log << "headerNameLT('" << tt.b << "' < '" << tt.a << "')='" << v2
          << "', expected '" << tt.rev << "'\n";
      return 2;
    }
  }

  return 0;
}

/**\brief Test header append function.
 *
 * \test Appending a header in HTTP means either setting the header or adding a
 *     comma and then the additional value. This test exercises that, and also
 *     exercises whether the type is properly case insensitive.
 *
 * \param[out] log
 * \returns Zero on success, nonzero otherwise.
 */
int testAppend(std::ostream &log) {
  struct sampleData {
    headers in;
    std::string key, value, out;
    bool res;
  };

  std::vector<sampleData> tests{
      {{}, "a", "b", "a: b\r\n", false},
      {{{"a", "b"}}, "a", "c", "a: b,c\r\n", true},
      {{{"a", "b"}, {"A", "c"}}, "A", "d", "a: b,d\r\n", true},
      {{{"a", "b"}, {"c", "d"}}, "a", "e", "a: b,e\r\nc: d\r\n", true},
  };

  for (const auto &tt : tests) {
    auto h = tt.in;
    const auto a = append(h, tt.key, tt.value);
    const auto v = to_string(h);
    if (v != tt.out) {
      log << "to_string()='" << v << "', expected '" << tt.out << "'\n";
      return 1;
    }
    if (a != tt.res) {
      log << "append(" << tt.out << ") had the wrong return value\n";
      return 2;
    }
  }

  return 0;
}

/**\brief Test header line parsing.
 *
 * \test This test appends single header lines, with various end-of-line
 *     characters, to prepared headers and verifies that they parsed correctly
 *     by turning the result into a flat header string.
 *
 * \param[out] log
 * \returns Zero on success, nonzero otherwise.
 */
int testAbsorb(std::ostream &log) {
  struct sampleData {
    headers in;
    std::string line, last, out, res;
  };

  std::vector<sampleData> tests{
      {{}, "a: b", "", "a: b\r\n", "a"},
      {{}, "a: b\r\n", "b", "a: b\r\n", "a"},
      {{{"a", "b"}}, "a: c", "a", "a: b,c\r\n", "a"},
      {{{"a", "b"}}, "\td\r\n", "a", "a: b,d\r\n", "a"},
      {{{"a", "b"}, {"c", "d"}}, "a: e\r", "e", "a: b,e\r\nc: d\r\n", "a"},
  };

  for (const auto &tt : tests) {
    auto h = tt.in;
    const auto a = absorb(h, tt.line, tt.last);
    const auto v = to_string(h);
    if (v != tt.out) {
      log << "to_string()='" << v << "', expected '" << tt.out << "'\n";
      return 1;
    }
    if (a != tt.res) {
      log << "absorb(" << tt.out << ") had the wrong return value: '" << a
          << "'\n";
      return 2;
    }
  }

  return 0;
}

TEST_BATCH(testToString, testCompare, testAppend, testAbsorb)

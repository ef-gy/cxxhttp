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

int testCompare(std::ostream &log) {
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
    const auto v = comparator::headerNameLT()(tt.a, tt.b);
    if (v != tt.res) {
      log << "headerNameLT('"<<tt.a<<"' < '"<<tt.b<<"')='" << v << "', expected '" << tt.res << "'\n";
      return 1;
    }
    const auto v2 = comparator::headerNameLT()(tt.b, tt.a);
    if (v2 != tt.rev) {
      log << "headerNameLT('"<<tt.b<<"' < '"<<tt.a<<"')='" << v2 << "', expected '" << tt.rev << "'\n";
      return 2;
    }
  }

  return 0;
}

TEST_BATCH(testToString, testCompare)

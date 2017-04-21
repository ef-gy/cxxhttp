/**\file
 * \brief Test cases for the HTTP/1.1 negotiation algorithm.
 *
 * This file contains unit tests for HTTP content negotiation, which is
 * implemented in cxxhttp/negotiate.h.
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

#include <cxxhttp/negotiate.h>

using namespace cxxhttp;

/**\brief Test string trim function.
 *
 * \test Header fields need to ignore a lot of white space; the trim() function
 *     removes that from both ends of a string, so this is a simple test to make
 *     sure that works.
 *
 * \param[out] log
 * \returns Zero on success, nonzero otherwise.
 */
int testTrim(std::ostream &log) {
  struct sampleData {
    std::string in, out;
  };

  std::vector<sampleData> tests{
      {"", ""},    {"a", "a"},   {" a", "a"},
      {"a ", "a"}, {" a ", "a"}, {" a b ", "a b"},
  };

  for (const auto &tt : tests) {
    const auto v = trim(tt.in);
    if (v != tt.out) {
      log << "trim('" << tt.in << "')='" << v << "', expected '" << tt.out
          << "'\n";
      return 1;
    }
  }

  return 0;
}

/**\brief Test list splitting.
 *
 * \test HTTP negotiation headers, like many others, are comma-separated, and
 *     the actual values are semicolon-separated. MIME has slash-separated
 *     types. This function exercises the split() function, which can use an
 *     arbitrary character and return a split list with that character as the
 *     separator.
 *
 * \param[out] log
 * \returns Zero on success, nonzero otherwise.
 */
int testSplit(std::ostream &log) {
  struct sampleData {
    std::string in;
    std::vector<std::string> out;
    std::vector<std::string> outSemi;
  };

  std::vector<sampleData> tests{
      {"", {}, {}},
      {"x", {"x"}, {"x"}},
      {"x, y, z;q=0", {"x", "y", "z;q=0"}, {"x, y, z", "q=0"}},
  };

  for (const auto &tt : tests) {
    const auto v = split(tt.in);
    if (v != tt.out) {
      log << "split('" << tt.in << "') has unexpected value.\n";
      return 1;
    }
    const auto v2 = split(tt.in, ';');
    if (v2 != tt.outSemi) {
      log << "split('" << tt.in << "', ';') has unexpected value.\n";
      return 1;
    }
  }

  return 0;
}

/**\brief Test q-value parsing.
 *
 * \test This function tests whether q-values are parsed correctly from list
 *     elements. This is done by first splitting the values and then recombining
 *     them in various ways.
 *
 * \param[out] log
 * \returns Zero on success, nonzero otherwise.
 */
int testQvalue(std::ostream &log) {
  struct sampleData {
    std::string in, recombined, full, value;
    std::set<std::string> attributes, extensions;
    int q;
  };

  std::vector<sampleData> tests{
      {"", "", "", "", {}, {}, 0},
      {"foo", "foo", "foo;q=1", "foo", {}, {}, 1000},
      {"b;q=0.2", "b", "b;q=0.2", "b", {}, {}, 200},
      {"a;q=0.3", "a", "a;q=0.3", "a", {}, {}, 300},
      {"foo;q=0.5", "foo", "foo;q=0.5", "foo", {}, {}, 500},
      {"text/html;level=1",
       "text/html;level=1",
       "text/html;level=1;q=1",
       "text/html",
       {"level=1"},
       {},
       1000},
      {" text/html ; level=1 ",
       "text/html;level=1",
       "text/html;level=1;q=1",
       "text/html",
       {"level=1"},
       {},
       1000},
      {"text/html;level=1;q=0.75",
       "text/html;level=1",
       "text/html;level=1;q=0.75",
       "text/html",
       {"level=1"},
       {},
       750},
      {" text/html ; level=1 ; q = 0.75 ",
       "text/html;level=1",
       "text/html;level=1;q=0.75",
       "text/html",
       {"level=1"},
       {},
       750},
      {"text/html;level=1;q=0.75;ext",
       "text/html;level=1",
       "text/html;level=1;q=0.75;ext",
       "text/html",
       {"level=1"},
       {"ext"},
       750},
      {"text/html;q=0.75;ext",
       "text/html",
       "text/html;q=0.75;ext",
       "text/html",
       {},
       {"ext"},
       750},
  };

  for (const auto &tt : tests) {
    const qvalue v = tt.in;
    if (std::string(v) != tt.recombined) {
      log << "qvalue('" << tt.in << "').recombined='" << std::string(v)
          << "', expected '" << tt.recombined << "'\n";
      return 1;
    }
    if (v.full() != tt.full) {
      log << "qvalue('" << tt.in << "').full()='" << v.full() << "', expected '"
          << tt.full << "'\n";
      return 1;
    }
    if (v.value != tt.value) {
      log << "qvalue('" << tt.in << "').value='" << v.value << "', expected '"
          << tt.value << "'\n";
      return 2;
    }
    if (v.q != tt.q) {
      log << "qvalue('" << tt.in << "').q='" << v.q << "', expected '" << tt.q
          << "'\n";
      return 3;
    }
    if (v.attributes != tt.attributes) {
      log << "qvalue('" << tt.in << "').attributes has an unexpected value\n";
      return 4;
    }
    if (v.extensions != tt.extensions) {
      log << "qvalue('" << tt.in << "').extensions has an unexpected value\n";
      return 4;
    }
  }

  return 0;
}

/**\brief Test q-value sorting.
 *
 * \test Q-values are sorted based on the 'q' parameter alone. This function
 *     makes sure that actually works, in theory.
 *
 * \param[out] log
 * \returns Zero on success, nonzero otherwise.
 */
int testQvalueLessThan(std::ostream &log) {
  struct sampleData {
    std::string a, b;
    bool isLess;
  };

  std::vector<sampleData> tests{
      {"a;q=0", "a;q=1", true},
      {"a;q=1", "a;q=0", false},
      {"a;q=1", "a;q=1", false},
      {"a", "b", true},
      {"b", "a", false},
      {"b", "a", false},
      {"a;q=0.3", "b;q=0.2", false},
      {"b;q=0.2", "a;q=0.3", true},
  };

  for (const auto &tt : tests) {
    const qvalue a = tt.a;
    const qvalue b = tt.b;
    if ((a < b) != tt.isLess) {
      log << "qvalue('" << tt.a << "' < '" << tt.b
          << "') has unexpected value.\n";
      return 1;
    }
  }

  return 0;
}

/**\brief Test q-value sorting in container.
 *
 * \test This is the practical counterpart to testQvalueLessThan(), which uses
 *     the sorting operation in a std::set container, which in turn uses the
 *     less-than operator to deduplicate entries.
 *
 * \param[out] log
 * \returns Zero on success, nonzero otherwise.
 */
int testQvalueSort(std::ostream &log) {
  struct sampleData {
    std::set<std::string> in;
    std::vector<std::string> out;
  };

  std::vector<sampleData> tests{
      {{}, {}},
      {{"a", "b"}, {"a", "b"}},
      {{"a", "a"}, {"a"}},
      {{"a;q=0.5", "a"}, {"a;q=0.5", "a"}},
      {{"a", "*"}, {"*", "a"}},
      {{"*;q=0.2", "a;q=0.1"}, {"a;q=0.1", "*;q=0.2"}},
      {{"b;q=0.2", "a;q=0.3"}, {"b;q=0.2", " a;q=0.3"}},
      {{"a;q=0.3", "b;q=0.2"}, {"b;q=0.2", " a;q=0.3"}},
  };

  for (const auto &tt : tests) {
    std::set<qvalue> in(tt.in.begin(), tt.in.end());
    std::vector<qvalue> out(tt.out.begin(), tt.out.end());
    std::vector<qvalue> v(in.begin(), in.end());
    if (out != v) {
      log << "unexpected sorting results:\n";
      for (const auto &a : in) {
        log << " * " << a.full() << "\n";
      }
      log << "expected:\n";
      for (const auto &a : out) {
        log << " * " << a.full() << "\n";
      }
      return 1;
    }
  }

  return 0;
}

/**\brief Test q-value matching.
 *
 * \test Standard less-than and equality relations don't necessarily hold true
 *     for these values, as sorting is performed primarily on q-value, whereas
 *     matching is performed based on the string value. This tests the equality
 *     relation defined on type.
 *
 * \param[out] log
 * \returns Zero on success, nonzero otherwise.
 */
int testQvalueMatch(std::ostream &log) {
  struct sampleData {
    std::string a, b;
    bool isMatch, aWildcard, bWildcard;
  };

  std::vector<sampleData> tests{
      {"", "", true, false, false},
      {"a/b", "a/*", true, false, true},
      {"*", "foo", true, true, false},
      {"a", "foo", false, false, false},
      {"a", "a;q=0.1", true, false, false},
      {"a", "b;q=0.2", false, false, false},
      {"*", "foo;bar", true, true, false},
      {"*;baz", "foo", true, true, false},
      {"a/b", "*/*", true, false, true},
      {"a/b", "a/b;c=d", false, false, false},
      {"a/*", "a/b;c=d", true, true, false},
      {"*/*", "a/b;c=d", true, true, false},
  };

  for (const auto &tt : tests) {
    const qvalue a = tt.a;
    const qvalue b = tt.b;
    if ((a == b) != tt.isMatch) {
      log << "qvalue('" << tt.a << "' == '" << tt.b
          << "') has unexpected value.\n";
      return 1;
    }
    if (a.hasWildcard() != tt.aWildcard) {
      log << "qvalue('" << tt.a << "').hasWildcard()=" << tt.aWildcard
          << " has unexpected value.\n";
      return 2;
    }
    if (b.hasWildcard() != tt.bWildcard) {
      log << "qvalue('" << tt.b << "').hasWildcard()=" << tt.bWildcard
          << " has unexpected value.\n";
      return 3;
    }
  }

  return 0;
}

/**\brief Test actual end-to-end negotiation.
 *
 * \test This function tests the actual negotiation algorithm, which pretty much
 *     ties together the rest of the tests in this file.
 *
 * \param[out] log
 * \returns Zero on success, nonzero otherwise.
 */
int testFullNegotiation(std::ostream &log) {
  struct sampleData {
    std::string theirs, mine, result, rresult;
  };

  std::vector<sampleData> tests{
      {"", "", "", ""},
      {"", "a", "a", ""},
      {"", "a/*", "", ""},
      {"", "a/*, a/b;q=0.1", "a/b", ""},
      {"", "a;q=0.1, b;q=0.2", "b", ""},
      {"", "a;q=0.3, b;q=0.2", "a", ""},
      {"a", "a;q=0.1, b;q=0.2", "a", "a"},
      {"*", "a;q=0.1, b;q=0.2", "b", "b"},
      {"a/c;q=0.2", "a/*, a/b;q=0.1", "a/c", "a/c"},
      {"foo/*", "foo/bar;q=0.1, b;q=0.2", "foo/bar", "foo/bar"},
      {"foo/*", "foo/bar;q=0.1, *;q=0.2", "foo/bar", "foo/bar"},
      // this is an example string from RFC 2616
      {"text/*;q=0.3, text/html;q=0.7, text/html;level=1,"
       "text/html;level=2;q=0.4, */*;q=0.5",
       "text/plain", "text/plain", "text/plain"},
      {"text/*;q=0.3, text/html;q=0.7, text/html;level=1,"
       "text/html;level=2;q=0.4, */*;q=0.5",
       "text/*", "text/html;level=1", "text/html;level=1"},
      {"text/*;q=0.3, text/html;q=0.7, text/html;level=1,"
       "text/html;level=2;q=0.4, */*;q=0.5",
       "text/*;q=0.1, text/html", "text/html", "text/html"},
  };

  for (const auto &tt : tests) {
    const std::string v = negotiate(tt.theirs, tt.mine);
    if (v != tt.result) {
      log << "negotiate('" << tt.theirs << "','" << tt.mine << "')='" << v
          << "', expected '" << tt.result << "'.\n";
      return 1;
    }
    const std::string v2 = negotiate(tt.mine, tt.theirs);
    if (v2 != tt.rresult) {
      log << "negotiate('" << tt.mine << "','" << tt.theirs << "')='" << v2
          << "', expected '" << tt.rresult << "'.\n";
      return 2;
    }
  }

  return 0;
}

TEST_BATCH(testTrim, testSplit, testQvalue, testQvalueLessThan, testQvalueSort,
           testQvalueMatch, testFullNegotiation)

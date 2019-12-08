/* Test cases for some of the grammar rules.
 *
 * HTTP is described in a modified ABNF, which for this library was translated
 * to regular expressions. Since the two aren't the same, this tests some of the
 * grammar rules to ensure they allow things that ought to be allowed and reject
 * things that aren't allowed.
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

#include <cxxhttp/http-grammar.h>
#include <ef.gy/test-case.h>

#include <regex>

using namespace cxxhttp;

/* Test grammar matches.
 * @log Test output stream.
 *
 * Tests some of the more complicated grammar rules, to make sure there's no odd
 * surprises further down the road.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testGrammar(std::ostream &log) {
  struct sampleData {
    std::string in, reference;
    bool result;
  };

  std::vector<sampleData> tests{
      {"", "", true},
      {"a", "", false},
      {http::grammar::vchar, "a", true},
      {http::grammar::vchar, "\n", false},
      {http::grammar::vchar, "\t", false},
      {http::grammar::quotedPair, "\\t", true},
      {http::grammar::quotedPair, "\\\"", true},
      {http::grammar::quotedPair, "a", false},
      {http::grammar::quotedPair, "\"", false},
      {http::grammar::qdtext, "a", true},
      {http::grammar::qdtext, ",", true},
      {http::grammar::qdtext, "[", true},
      {http::grammar::qdtext, "\\", false},
      {http::grammar::qdtext, "]", true},
      {http::grammar::qdtext, "\\", false},
      {http::grammar::qdtext, "\"", false},
      {http::grammar::quotedString, "\"\"", true},
      {http::grammar::quotedString, "\"foo\"\"", false},
      {http::grammar::quotedString, "\"foo\"bar\"", false},
      {http::grammar::quotedString, "\"foo=\"bar\"\"", false},
      {http::grammar::quotedString, "\"foo=\\\"bar\\\"\"", true},
      {http::grammar::comment, "(foo)", true},
      {http::grammar::comment, "(foo!)", true},
      {http::grammar::comment, "(foo (bar))", true},
      {http::grammar::token, "foo", true},
      {http::grammar::token, "foo-B4r", true},
      {http::grammar::token, "foo-B4r ", false},
      {http::grammar::token, " ", false},
      {http::grammar::token, "", false},
      {http::grammar::fieldContent, "fo  of", true},
      {http::grammar::fieldContent, "fo", true},
      {http::grammar::fieldContent, " foof ", false},
  };

  for (const auto &tt : tests) {
    try {
      const std::regex rx(tt.in);
      const auto v = std::regex_match(tt.reference, rx);
      if (v != tt.result) {
        log << "regex_match('" << tt.reference << "', '" << tt.in << "')='" << v
            << "', expected '" << tt.result << "'\n";
        return false;
      }
    } catch (std::exception &e) {
      log << "Exception: " << e.what() << "\nWhile trying to test: " << tt.in
          << "\n";
      return false;
    }
  }

  return true;
}

namespace test {
using efgy::test::function;

static function grammar(testGrammar);
}  // namespace test

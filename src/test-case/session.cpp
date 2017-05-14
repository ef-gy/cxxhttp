/* Test cases for basic HTTP session handling.
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

#define ASIO_DISABLE_THREADS
#include <ef.gy/test-case.h>

#include <cxxhttp/http-session.h>

using namespace cxxhttp;

/* Test basic session attributes.
 * @log Test output stream.
 *
 * Test cases for some of the basic session functions.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testBasicSession(std::ostream &log) {
  struct sampleData {
    std::size_t requests, replies, contentLength;
    std::string content;
    std::size_t queries, remainingBytes;
  };

  std::vector<sampleData> tests{
      {1, 2, 500, "foo", 3, 497},
  };

  for (const auto &tt : tests) {
    http::sessionData v;

    v.requests = tt.requests;
    v.replies = tt.replies;
    v.contentLength = tt.contentLength;
    v.content = tt.content;

    if (v.queries() != tt.queries) {
      log << "queries() = " << v.queries() << ", but expected " << tt.queries
          << "\n";
      return false;
    }

    if (v.remainingBytes() != tt.remainingBytes) {
      log << "queries() = " << v.remainingBytes() << ", but expected "
          << tt.remainingBytes << "\n";
      return false;
    }
  }

  return true;
}

namespace test {
using efgy::test::function;

static function basicSession(testBasicSession);
}

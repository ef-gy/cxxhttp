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
    enum http::status status;
    std::size_t requests, replies, contentLength;
    std::string content;
    std::size_t queries, remainingBytes;
    std::string buffer;
  };

  std::vector<sampleData> tests{
      {http::stRequest, 1, 2, 500, "foo", 3, 497, ""},
      {http::stContent, 1, 2, 500, "foo", 3, 497, ""},
      {http::stShutdown, 1, 2, 500, "foo", 3, 497, ""},
  };

  for (const auto &tt : tests) {
    http::sessionData v;

    v.status = tt.status;
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

    if (v.buffer() != tt.buffer) {
      log << "buffer() = " << v.buffer() << ", but expected " << tt.buffer
          << "\n";
      return false;
    }
  }

  return true;
}

/* Test log line creation.
 * @log Test output stream.
 *
 * Creates log lines for a few sample sessions.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testLog(std::ostream &log) {
  struct sampleData {
    std::string request;
    http::headers header;
    int status;
    std::size_t length;
    std::string log;
  };

  std::vector<sampleData> tests{
      {"GET / HTTP/1.1",
       {},
       200,
       42,
       "{\"length\":42,\"method\":\"GET\",\"protocol\":\"HTTP/1.1\","
       "\"resource\":\"/\",\"status\":200}"},
      {"GET / HTTP/1.1",
       {{"User-Agent", "frob/123"}},
       200,
       42,
       "{\"length\":42,\"method\":\"GET\",\"protocol\":\"HTTP/1.1\","
       "\"resource\":\"/\",\"status\":200,\"user-agent\":\"frob/123\"}"},
      {"GET / HTTP/1.1",
       {{"Referer", "http://foo/"}},
       200,
       42,
       "{\"length\":42,\"method\":\"GET\",\"protocol\":\"HTTP/1.1\","
       "\"referer\":\"http://foo/\",\"resource\":\"/\",\"status\":200}"},
  };

  for (const auto &tt : tests) {
    http::sessionData s;

    s.inboundRequest = tt.request;
    s.inbound.header = tt.header;

    const auto &v = s.logMessage(tt.status, tt.length);

    if (v != tt.log) {
      log << "logMessage() = '" << v << "', but expected '" << tt.log << "'\n";
      return false;
    }
  }

  return true;
}

/* Test request body creation.
 * @log Test output stream.
 *
 * Create sample replies for a few requests and verify them against known data.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testReply(std::ostream &log) {
  struct sampleData {
    int status;
    http::headers header;
    std::string body, message;
  };

  std::vector<sampleData> tests{
      {100, {}, "", "HTTP/1.1 100 Continue\r\n\r\n"},
      {100, {}, "ignored", "HTTP/1.1 100 Continue\r\n\r\n"},
      {200, {}, "foo", "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nfoo"},
      {404,
       {},
       "sorry",
       "HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: "
       "5\r\n\r\nsorry"},
  };

  for (const auto &tt : tests) {
    http::sessionData s;

    const auto &v = s.generateReply(tt.status, tt.body, tt.header);

    if (v != tt.message) {
      log << "generateReply() = '" << v << "', but expected '" << tt.message
          << "'\n";
      return false;
    }
  }

  return true;
}

/* Test server-side session header negotiation.
 * @log Test output stream.
 *
 * Creates sample sessions and tries to negotiate headers.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testNegotiate(std::ostream &log) {
  struct sampleData {
    http::headers inbound, negotiations, outbound, negotiated;
    bool success;
  };

  std::vector<sampleData> tests{
      {{{"Foo", "Bar"}},
       {{"Baz", "blorb"}},
       {{"Vary", "Baz"}},
       {{"Baz", "blorb"}},
       true},
      {{{"Accept", "foo/*"}},
       {{"Accept", "foo/bar, frob/blorgh"}},
       {{"Content-Type", "foo/bar"}, {"Vary", "Accept"}},
       {{"Accept", "foo/bar"}},
       true},
      {{},
       {{"Accept", "foo/bar, frob/blorgh;q=0.9"}},
       {{"Content-Type", "foo/bar"}, {"Vary", "Accept"}},
       {{"Accept", "foo/bar"}},
       true},
      {{{"Accept", "bar/*"}},
       {{"Accept", "foo/bar, frob/blorgh"}},
       {{"Vary", "Accept"}},
       {},
       false},
  };

  for (const auto &tt : tests) {
    http::sessionData s;

    s.inbound.header = tt.inbound;
    bool success = s.negotiate(tt.negotiations);

    if (success != tt.success) {
      log << "negotiate()=" << success << ", but expected " << tt.success
          << "\n";
      return false;
    }
    if (success) {
      if (s.outbound.header != tt.outbound || s.negotiated != tt.negotiated) {
        log << "negotiate() failed to produce the expected result.\n";
        return false;
      }
    }
  }

  return true;
}

/* Test whether a 405 is sent at an appropriate time.
 * @log Test output stream.
 *
 * `sessionData::trigger405` decides whether to send a 405 or a 404 and this
 * test exercises that function.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testTrigger405(std::ostream &log) {
  struct sampleData {
    std::set<std::string> methods;
    bool result;
  };

  std::vector<sampleData> tests{
      {{}, false},        {{"OPTIONS"}, false},
      {{"TRACE"}, false}, {{"OPTIONS", "GET"}, true},
      {{"GET"}, true},
  };

  for (const auto &tt : tests) {
    const bool r = http::sessionData::trigger405(tt.methods);

    if (r != tt.result) {
      log << "trigger405()=" << r << ", but expected " << tt.result << "\n";
      return false;
    }
  }

  return true;
}

namespace test {
using efgy::test::function;

static function basicSession(testBasicSession);
static function log(testLog);
static function reply(testReply);
static function negotiate(testNegotiate);
static function trigger405(testTrigger405);
}

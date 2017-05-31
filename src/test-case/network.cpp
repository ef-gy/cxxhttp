/* Network handling tests.
 *
 * These tests are potentially flaky, in the sense that they rely more on the
 * networking setup of the host running them than other test.
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

#include <sstream>

#include <ef.gy/test-case.h>

#define ASIO_DISABLE_THREADS
#define NO_DEFAULT_OPTIONS
#include <cxxhttp/network.h>

using namespace cxxhttp;

/* Test address lookup.
 * @log Test output stream.
 *
 * Uses the net::endpoint to look up appropriate endpoints.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testLookup(std::ostream &log) {
  struct sampleData {
    std::string host, service;
    std::set<std::string> expected;
    int minCount;
  };

  std::vector<sampleData> tests{
      {"0.0.0.0", "80", {"0.0.0.0:80"}, 1},
      {"localhost", "80", {"127.0.0.1:80", "::1:80"}, 1},
      {"localhost", "http", {"127.0.0.1:80", "::1:80"}, 1},
      {"localhost", "ftp", {"127.0.0.1:21", "::1:21"}, 1},
  };

  for (const auto &tt : tests) {
    const auto v = net::endpoint<transport::tcp>(tt.host, tt.service);
    int c = 0;
    for (net::endpointType<transport::tcp> a : v) {
      const std::string r = net::address(a);
      if (tt.expected.find(r) == tt.expected.end()) {
        log << "unexpected lookup result: " << r << " for host '" << tt.host
            << "' and service '" << tt.service << "'\n";
        return false;
      }
      c++;
    }

    if (c < tt.minCount) {
      log << "not enough results; got " << c << " but expected at least "
          << tt.minCount << " for host '" << tt.host << "' and service '"
          << tt.service << "'\n";
      return false;
    }
  }

  service io;
  const auto d = transport::tcp::socket(io);
  if (net::address(d) != "[UNAVAILABLE]") {
    log << "unix socket name lookup has unexpected result: " << net::address(d)
        << "\n";
    return false;
  }

  const auto v = net::endpoint<transport::unix>("/tmp/random-socket");
  if (v.size() != 1) {
    log << "unix socket lookup count is " << v.size() << " but should be 1.\n";
    return false;
  }

  const auto ve = transport::unix::endpoint(v[0]);
  if (net::address(ve) != "/tmp/random-socket") {
    log << "unix socket lookup has unexpected address: " << net::address(ve)
        << "\n";
    return false;
  }

  const auto e = transport::unix::socket(io);
  if (net::address(e) != "[UNAVAILABLE]") {
    log << "unix socket name lookup has unexpected result: " << net::address(e)
        << "\n";
    return false;
  }

  const auto ee = transport::unix::endpoint();
  if (net::address(ee) != "[UNIX:empty]") {
    log << "unix socket name lookup has unexpected result: " << net::address(ee)
        << "\n";
    return false;
  }

  return true;
}

namespace test {
using efgy::test::function;

static function lookup(testLookup);
}

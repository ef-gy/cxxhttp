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

#include <ef.gy/test-case.h>

#include <sstream>

#define ASIO_DISABLE_THREADS
#define NO_DEFAULT_OPTIONS
#include <cxxhttp/network.h>

using namespace cxxhttp;

/* Resolve UNIX endpoint address.
 * @endpoint The endpoint to examine.
 *
 * Examines a UNIX endpoint to resolve its address.
 *
 * @return The address of the endpoint.
 */
static inline std::string address(const transport::unix::endpoint &endpoint) {
  if (endpoint.path().empty()) {
    return "[UNIX:empty]";
  } else {
    return endpoint.path();
  }
}

/* Resolve UNIX socket address.
 * @socket The UNIX socket wrapper.
 *
 * Pretty much the same implementation as the for TCP sockets: look up the
 * remote endpoint of that and turn it into a string.
 *
 * @return The remote address of the socket.
 */
static inline std::string address(const transport::unix::socket &socket) {
  asio::error_code ec;
  const auto endpoint = socket.remote_endpoint(ec);
  if (ec) {
    return "[UNAVAILABLE]";
  } else {
    return address(endpoint);
  }
}

/* Resolve TCP endpoint address.
 * @endpoint The endpoint to examine.
 *
 * Examines a TCP endpoint to resolve its address.
 *
 * @return The address of the endpoint.
 */
static inline std::string address(const transport::tcp::endpoint &endpoint) {
  return endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
}

/* Resolve TCP socket address.
 * @socket The socket to examine.
 *
 * Examines a TCP socket to resolve the host address on the other end.
 *
 * @return The address of whatever the socket is connected to.
 */
static inline std::string address(const transport::tcp::socket &socket) {
  asio::error_code ec;
  const auto endpoint = socket.remote_endpoint(ec);
  if (ec) {
    return "[UNAVAILABLE]";
  } else {
    return address(endpoint);
  }
}

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
      const std::string r = address(a);
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
  if (address(d) != "[UNAVAILABLE]") {
    log << "unix socket name lookup has unexpected result: " << address(d)
        << "\n";
    return false;
  }

  const auto v = net::endpoint<transport::unix>("/tmp/random-socket");
  if (v.size() != 1) {
    log << "unix socket lookup count is " << v.size() << " but should be 1.\n";
    return false;
  }

  const auto ve = transport::unix::endpoint(v[0]);
  if (address(ve) != "/tmp/random-socket") {
    log << "unix socket lookup has unexpected address: " << address(ve) << "\n";
    return false;
  }

  const auto e = transport::unix::socket(io);
  if (address(e) != "[UNAVAILABLE]") {
    log << "unix socket name lookup has unexpected result: " << address(e)
        << "\n";
    return false;
  }

  const auto ee = transport::unix::endpoint();
  if (address(ee) != "[UNIX:empty]") {
    log << "unix socket name lookup has unexpected result: " << address(ee)
        << "\n";
    return false;
  }

  return true;
}

/* Test session and connection recycling invariants.
 * @log Test output stream.
 *
 * Creates a few connections with sessions and examines some invariants on them.
 *
 * @return 'true' on success, 'false' otherwise.
 */
bool testRecycling(std::ostream &log) {
  service io;
  struct acc {
    acc(const service &){};
  };
  struct tr {
    using endpoint = int;
    using acceptor = acc;
  };
  struct proc {};
  struct sess;

  using conn = net::connection<sess, proc>;
  struct sess {
    using transportType = tr;

    bool free = false;

    sess(conn &c) : beacon(*this, c.sessions) {}

    efgy::beacon<sess> beacon;
  };

  efgy::beacons<conn> conns;

  if (!conns.empty()) {
    log << "expected no connections after initialisation.\n";
    return false;
  }

  {
    // explicitly force the emission of a destructor by making this go out of
    // scope. It'd also do this in the main scope of the function, but I like to
    // think this makes it explicit - and we can test a post-condition.
    conn c(conns, io);

    if (conns.size() != 1) {
      log << "expected one connection after constructor ran.\n";
      return false;
    }

    if (!c.idle()) {
      log << "expected connection to be idle immediately after construction.\n";
      return false;
    }

    auto s = c.getSession();

    if (s->free) {
      log << "new session should not have been free.\n";
      return false;
    }

    if (c.idle()) {
      log << "new session should not be idle after creating session.\n";
      return false;
    }

    c.pending = true;
    s->free = true;

    if (c.idle()) {
      log << "connection should not be idle after setting the pending flag.\n";
      return false;
    }

    c.pending = false;

    if (!c.idle()) {
      log << "connection should idle again after setting pending to false.\n";
      return false;
    }
  }

  if (!conns.empty()) {
    log << "expected no connections after connection destructor ran.\n";
    return false;
  }

  return true;
}

namespace test {
using efgy::test::function;

static function lookup(testLookup);
static function recycling(testRecycling);
}  // namespace test

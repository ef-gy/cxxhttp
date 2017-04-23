/* Basic network management building blocks.
 *
 * For all those little things that networking needs.
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
#if !defined(CXXHTTP_NETWORK_H)
#define CXXHTTP_NETWORK_H

#include <functional>
#include <iostream>
#include <sstream>
#include <string>

#define ASIO_STANDALONE
#include <asio.hpp>

#include <ef.gy/cli.h>
#include <ef.gy/global.h>
#include <ef.gy/version.h>

namespace cxxhttp {
/* asio::io_service type.
 *
 * We declare this here so expicitly declare support for this.
 */
using service = asio::io_service;

// define USE_DEFAULT_IO_MAIN to use this main function, or just call it.
#if defined(USE_DEFAULT_IO_MAIN)
#define IO_MAIN_SPEC extern "C"
#else
#define IO_MAIN_SPEC static
#endif

/* Default IO main function.
 * @argc Argument count.
 * @argv Argument vector.
 *
 * Applies all arguments with the efgy::cli facilities, then (tries to) run an
 * ASIO I/O loop.
 *
 * @return 0 on success, -1 on failure.
 */
IO_MAIN_SPEC int main(int argc, char *argv[]) {
  efgy::cli::options opts(argc, argv);

  efgy::global<service>().run();

  return opts.matches == 0 ? -1 : 0;
}

namespace transport {
using tcp = asio::ip::tcp;
using unix = asio::local::stream_protocol;
}

namespace net {
/* Resolve UNIX socket address.
 *
 * This is currently not supported, so will just return a string to indicate
 * that it's a UNIX socket and not something else.
 *
 * @return A fixed string that says that this is a UNIX socket.
 */
std::string address(const transport::unix::socket &) { return "[UNIX]"; }

/* Resolve TCP socket address.
 * @socket The socket to examine.
 *
 * Examines a TCP socket to resolve the host address on the other end.
 *
 * @return The address of whatever the socket is connected to.
 */
std::string address(const transport::tcp::socket &socket) {
  return socket.remote_endpoint().address().to_string();
}

template <typename base = transport::unix>
class endpoint {
 public:
  endpoint(const std::string &pSocket) : socket(pSocket) {}

  std::size_t with(std::function<bool(typename base::endpoint &)> f) {
    typename base::endpoint endpoint(socket);
    if (f(endpoint)) {
      return 1;
    }

    return 0;
  }

  const std::string &name(void) const { return socket; }

 protected:
  const std::string socket;
};

template <>
class endpoint<transport::tcp> {
 public:
  endpoint(const std::string &pHost, const std::string &pPort,
           service &pService = efgy::global<cxxhttp::service>())
      : host(pHost), port(pPort), service(pService) {}

  std::size_t with(std::function<bool(transport::tcp::endpoint &)> f) {
    std::size_t res = 0;

    transport::tcp::resolver resolver(service);
    transport::tcp::resolver::query query(host, port);
    transport::tcp::resolver::iterator endpoint_iterator =
        resolver.resolve(query);
    transport::tcp::resolver::iterator end;

    if (endpoint_iterator != end) {
      transport::tcp::endpoint endpoint = *endpoint_iterator;
      if (f(endpoint)) {
        res++;
      }
    }

    return res;
  }

  const std::string &name(void) const { return host; }

 protected:
  const std::string host;
  const std::string port;
  service &service;
};

/**\brief Basic asynchronous connection wrapper
 *
 * Contains all the data relating to a particular connection - either for a
 * server, or a client.
 *
 * \tparam requestProcessor The functor class to handle requests.
 */
template <typename requestProcessor>
class connection {
 public:
  /**\brief Initialise with IO service
   *
   * Default constructor which binds an IO service and sets up a new processor.
   *
   * \param[out] pio      IO service to use.
   * \param[out] logfile  A stream to write log messages to.
   */
  connection(service &pio, std::ostream &logfile)
      : io(pio),
        log(logfile),
        processor(),
        name("connection"),
        version("cxxhttp/") {
    std::ostringstream ver("");
    ver << efgy::version;
    version = version + ver.str();
  }

  /**\brief Request processor instance
   *
   * Used to generate replies for incoming queries.
   */
  requestProcessor processor;

  /**\brief IO service
   *
   * Bound in the constructor to an asio.hpp IO service which handles
   * asynchronous connections.
   */
  service &io;

  /**\brief Log stream
   *
   * This is a standard output stream to send log data to. Log data is written
   * by the session code, so that code determines the format of log lines.
   */
  std::ostream &log;

  /**\brief Node name
   *
   * A name for the endpoint; some server protocols may need this. By default
   * this is populated with "server" in the constructor, so you have to set this
   * yourself if the kind of server you have needs this.
   */
  std::string name;

  /**\brief Software version
   *
   * A "software version" string, which is used by some protocols.
   * Defaults to "cxxhttp/<version>".
   */
  std::string version;
};
}
}

#endif

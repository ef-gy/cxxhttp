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
#include <ef.gy/version.h>

namespace cxxhttp {
namespace io {
/* asio::io_service wrapper
 *
 * The purpose of this object is to be able to have a 'default' IO service
 * object for server and client code to use. This is provided by the
 * service::common() function.
 */
class service {
 public:
  static service &common(void) {
    static service serv;
    return serv;
  }

  asio::io_service &get(void) { return io_service; }

  std::size_t run(void) { return io_service.run(); }

  int main(int argc, char *argv[]) {
    efgy::cli::options opts(argc, argv);

    run();

    return opts.matches == 0 ? -1 : 0;
  }

 protected:
  asio::io_service io_service;
};

static int main(int argc, char *argv[]) {
  return service::common().main(argc, argv);
}
}

namespace net {
namespace transport {
using tcp = asio::ip::tcp;
using unix = asio::local::stream_protocol;
}

template <typename S>
static std::string address(const S &socket) {
  return "[?]";
}

template <>
std::string address(const transport::unix::socket &socket) {
  return "[UNIX]";
}

template <>
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
           io::service &pService = io::service::common())
      : host(pHost), port(pPort), service(pService) {}

  std::size_t with(std::function<bool(transport::tcp::endpoint &)> f) {
    std::size_t res = 0;

    transport::tcp::resolver resolver(service.get());
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
  io::service &service;
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
  connection(io::service &pio, std::ostream &logfile)
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
  io::service &io;

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

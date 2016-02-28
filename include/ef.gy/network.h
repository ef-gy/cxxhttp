/**\file
 *
 * \copyright
 * This file is part of the libefgy project, which is released as open source
 * under the terms of an MIT/X11-style licence, described in the COPYING file.
 *
 * \see Project Documentation: https://ef.gy/documentation/libefgy
 * \see Project Source Code: https://github.com/ef-gy/libefgy
 * \see Licence Terms: https://github.com/ef-gy/libefgy/blob/master/COPYING
 */

#if !defined(EF_GY_NETWORK_H)
#define EF_GY_NETWORK_H

#include <functional>
#include <iostream>
#include <sstream>
#include <string>

#define ASIO_STANDALONE
#include <asio.hpp>

#include <ef.gy/cli.h>
#include <ef.gy/version.h>

namespace efgy {
namespace io {
/**\brief asio::io_service wrapper
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

  int main(int argc, char *argv[],
           cli::options<> &opts = cli::options<>::common()) {
    int rv = opts.apply(argc, argv) == 0;

    run();

    return rv;
  }

protected:
  asio::io_service io_service;
};

static int main(int argc, char *argv[]) {
  return service::common().main(argc, argv);
}
}

/**\brief Networking code
 *
 * Contains templates that deal with networking in one way or another. This code
 * is based on asio.hpp, an asynchronous I/O library for C++. The makefile knows
 * how to download this header-only library in case it's not installed.
 */
namespace net {
template <typename S> static std::string address(const S &socket) {
  return "[?]";
}

template <>
std::string address(const asio::local::stream_protocol::socket &socket) {
  return "[UNIX]";
}

template <> std::string address(const asio::ip::tcp::socket &socket) {
  return socket.remote_endpoint().address().to_string();
}

template <typename base = asio::local::stream_protocol> class endpoint {
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

template <> class endpoint<asio::ip::tcp> {
public:
  endpoint(const std::string &pHost, const std::string &pPort,
           io::service &pService = io::service::common())
      : host(pHost), port(pPort), service(pService) {}

  std::size_t with(std::function<bool(asio::ip::tcp::endpoint &)> f) {
    std::size_t res = 0;

    asio::ip::tcp::resolver resolver(service.get());
    asio::ip::tcp::resolver::query query(host, port);
    asio::ip::tcp::resolver::iterator endpoint_iterator =
        resolver.resolve(query);
    asio::ip::tcp::resolver::iterator end;

    if (endpoint_iterator != end) {
      asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
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
template <typename requestProcessor> class connection {
public:
  /**\brief Initialise with IO service
   *
   * Default constructor which binds an IO service and sets up a new processor.
   *
   * \param[out] pio      IO service to use.
   * \param[out] logfile  A stream to write log messages to.
   */
  connection(io::service &pio, std::ostream &logfile)
      : io(pio), log(logfile), processor(), name("connection"),
        version("libefgy/") {
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
   * Defaults to "libefgy/<version>".
   */
  std::string version;
};
}
}

#endif

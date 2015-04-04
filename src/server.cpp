/**\file
 * \ingroup example-programmes
 * \brief "Hello World" HTTP/IRC Server
 *
 * An example HTTP server that serves a simple "Hello World!" on /, and a 404 on
 * all other resources. The programme also contains a sample IRC server that can
 * run in parallel, or instead of, the HTTP server.
 *
 * Call it like this:
 * \code
 * $ ./hello http:localhost:8080
 * \endcode
 *
 * With localhost and 8080 being a host name and port of your choosing. Then,
 * while the programme is running, open a browser and go to
 * http://localhost:8080/ and you should see the familiar greeting.
 *
 * You can also use 'irc' in place of 'http'. This would spawn an IRC server,
 * which you can connect to and use with your favourite IRC client.
 *
 * \copyright
 * Copyright (c) 2015, ef.gy Project Members
 * \copyright
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * \copyright
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * \copyright
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * \see Project Documentation: https://ef.gy/documentation/libefgy
 * \see Project Source Code: https://github.com/ef-gy/libefgy
 */

#define ASIO_DISABLE_THREADS
#include <ef.gy/http.h>
#include <ef.gy/irc.h>

using namespace efgy;
using asio::ip::tcp;
using asio::local::stream_protocol;

namespace handle {
namespace tcp {
/**\brief Hello World request handler
 *
 * This function serves the familiar "Hello World!" when called.
 *
 * \param[out] session The HTTP session to answer on.
 *
 * \returns true (always, as we always reply).
 */
static bool hello(typename net::http::server<asio::ip::tcp>::session &session,
                  std::smatch &) {
  session.reply(200, "Hello World!");

  return true;
}

/**\brief Hello World request handler for /quit
 *
 * When this handler is invoked, it stops the ASIO IO handler (after replying,
 * maybe...).
 *
 * \note Having this on your production server in this exact way is PROBABLY a
 *       really bad idea, unless you gate it in an upstream forward proxy. Or
 *       you have some way of automatically respawning your server. Or both.
 *
 * \param[out] session The HTTP session to answer on.
 *
 * \returns true (always, as we always reply).
 */
static bool quit(typename net::http::server<asio::ip::tcp>::session &session,
                 std::smatch &) {
  session.reply(200, "Good-Bye, Cruel World!");

  session.server.io.stop();

  return true;
}
}

namespace unix {
/**\brief Hello World request handler
 *
 * This function serves the familiar "Hello World!" when called.
 *
 * \param[out] session The HTTP session to answer on.
 *
 * \returns true (always, as we always reply).
 */
static bool hello(typename net::http::server<stream_protocol>::session &session,
                  std::smatch &) {
  session.reply(200, "Hello World!");

  return true;
}

/**\brief Hello World request handler for /quit
 *
 * When this handler is invoked, it stops the ASIO IO handler (after replying,
 * maybe...).
 *
 * \note Having this on your production server in this exact way is PROBABLY a
 *       really bad idea, unless you gate it in an upstream forward proxy. Or
 *       you have some way of automatically respawning your server. Or both.
 *
 * \param[out] session The HTTP session to answer on.
 *
 * \returns true (always, as we always reply).
 */
static bool quit(typename net::http::server<stream_protocol>::session &session,
                 std::smatch &) {
  session.reply(200, "Good-Bye, Cruel World!");

  session.server.io.stop();

  return true;
}
}
}

/**\brief Main function for the HTTP/IRC demo
 *
 * Main function for the network server hello world programme.
 *
 * Sets up server(s) as per the given command line arguments. Invalid arguments
 * are ignored.
 *
 * \param[in] argc Process argument count.
 * \param[in] argv Process argument vector
 *
 * \returns 0 when nothing bad happened, 1 otherwise.
 */
int main(int argc, char *argv[]) {
  try {
    int targets = 0;

    if (argc < 2) {
      std::cerr << "Usage: irc-hello [irc:<host>:<port>|irc:unix:<path>|"
                   "http:<host>:<port>|http:unix:<path>]...\n";
      return 1;
    }

    asio::io_service io_service;
    tcp::resolver resolver(io_service);

    for (unsigned int i = 1; i < argc; i++) {
      static const std::regex http("http:(.+):([0-9]+)");
      static const std::regex httpSocket("http:unix:(.+)");
      static const std::regex irc("irc:(.+):([0-9]+)");
      static const std::regex ircSocket("irc:unix:(.+)");
      std::smatch matches;

      if (std::regex_match(std::string(argv[i]), matches, httpSocket)) {
        std::string socket = matches[1];

        stream_protocol::endpoint endpoint(socket);
        net::http::server<stream_protocol> *s =
            new net::http::server<stream_protocol>(io_service, endpoint);

        s->processor.add("^/$", handle::unix::hello);
        s->processor.add("^/quit$", handle::unix::quit);

        targets++;
      } else if (std::regex_match(std::string(argv[i]), matches, http)) {
        std::string host = matches[1];
        std::string port = matches[2];

        tcp::resolver::query query(host, port);
        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
        tcp::resolver::iterator end;

        if (endpoint_iterator != end) {
          tcp::endpoint endpoint = *endpoint_iterator;
          net::http::server<tcp> *s =
              new net::http::server<tcp>(io_service, endpoint);

          s->processor.add("^/$", handle::tcp::hello);
          s->processor.add("^/quit$", handle::tcp::quit);

          targets++;
        }
      } else if (std::regex_match(std::string(argv[i]), matches, ircSocket)) {
        std::string socket = matches[1];

        stream_protocol::endpoint endpoint(socket);
        net::irc::server<stream_protocol> *s =
            new net::irc::server<stream_protocol>(io_service, endpoint);

        s->name = socket;

        targets++;
      } else if (std::regex_match(std::string(argv[i]), matches, irc)) {
        std::string host = matches[1];
        std::string port = matches[2];

        tcp::resolver::query query(host, port);
        tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
        tcp::resolver::iterator end;

        if (endpoint_iterator != end) {
          tcp::endpoint endpoint = *endpoint_iterator;
          net::irc::server<tcp> *s =
              new net::irc::server<tcp>(io_service, endpoint);

          s->name = host;

          targets++;
        }
      } else {
        std::cerr << "Argument not recognised: " << argv[i] << "\n";
      }
    }

    if (targets > 0) {
      io_service.run();
    }

    return 0;
  }
  catch (std::exception & e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }
  catch (std::system_error & e) {
    std::cerr << "System Error: " << e.what() << "\n";
  }

  return 1;
}

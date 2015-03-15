/**\file
 * \brief "Hello World" HTTP Server
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
 * \see Project Documentation: http://ef.gy/documentation/libefgy
 * \see Project Source Code: https://github.com/ef-gy/libefgy
 */

#include <ef.gy/http.h>
#include <iostream>

using namespace efgy;
using namespace asio;
using namespace std;

using asio::ip::tcp;

class processHelloWorld {
public:
  bool operator()(net::http::session<tcp, processHelloWorld> &a) {
    a.reply(200, "Hello World!");

    return true;
  }
};

int main(int argc, char *argv[]) {
  try {
    if (argc != 3) {
      std::cerr << "Usage: http-hello <host> <port>\n";
      return 1;
    }

    asio::io_service io_service;
    tcp::resolver resolver(io_service);
    tcp::resolver::query query(argv[1], argv[2]);
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    tcp::resolver::iterator end;

    if (endpoint_iterator != end) {
      tcp::endpoint endpoint = *endpoint_iterator;
      net::http::server<tcp, processHelloWorld> s(io_service, endpoint);
      io_service.run();
    }
  } catch (std::exception & e) {
    std::cerr << "Exception: " << e.what() << "\n";
  } catch (std::system_error & e) {
    std::cerr << "System Error: " << e.what() << "\n";
  }

  return 0;
}

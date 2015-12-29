/**\file
 * \ingroup example-programmes
 * \brief FaaS: Fortune as a Service
 *
 * Call it like this:
 * \code
 * $ ./fortuned http:localhost:8080
 * \endcode
 *
 * With localhost and 8080 being a host name and port of your choosing. Then,
 * while the programme is running.
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
#include <ef.gy/fortuned.h>

using namespace efgy;

namespace tcp {
using asio::ip::tcp;
static httpd::servlet<tcp> fortune(fortuned::regex, fortuned::fortune<tcp>);
static httpd::servlet<tcp> quit("/quit", httpd::quit<tcp>);
}

namespace unix {
using asio::local::stream_protocol;
static httpd::servlet<stream_protocol>
    fortune(fortuned::regex, fortuned::fortune<stream_protocol>);
static httpd::servlet<stream_protocol> quit("/quit",
                                            httpd::quit<stream_protocol>);
}

int main(int argc, char *argv[]) {
  fortune::common().prepare("/usr/share/games/fortunes/");
  fortune::common().prepare("/usr/share/games/fortunes/off/", true);

  srand(time(0));

  return io::main(argc, argv);
}

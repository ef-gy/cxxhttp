/* End-to-end tests.
 *
 * This file is for full-on end-to-end tests. Eventually, anyway. Right now it's
 * just to make sure the server portions compile, and to make sure the coverage
 * reports look worse.
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

#include <cxxhttp/httpd-options.h>
#include <cxxhttp/httpd-trace.h>

using namespace cxxhttp;

namespace test {
using efgy::test::function;
}

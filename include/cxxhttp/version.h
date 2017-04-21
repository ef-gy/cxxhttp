/* Version Information.
 *
 * Contains cxxhttp's version number and User-Agent/Server string.
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
#if !defined(CXXHTTP_VERSION_H)
#define CXXHTTP_VERSION_H

#include <ef.gy/version.h>
#include <asio/version.hpp>
#include <string>

namespace cxxhttp {
static const unsigned int version = 1;

static const std::string identifier =
    "cxxhttp/" + std::to_string(version) + " asio/" +
    std::to_string(ASIO_VERSION) + " libefgy/" + std::to_string(efgy::version);
}

#endif

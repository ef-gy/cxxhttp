/**\file
 * \brief Version information
 *
 * Contains cxxhttp's version number and assorted pieces of documentation
 * that didn't quite fit in anywhere else.
 *
 * \copyright
 * This file is part of the cxxhttp project, which is released as open source
 * under the terms of an MIT/X11-style licence, described in the COPYING file.
 *
 * \see Project Documentation: http://ef.gy/documentation/cxxhttp
 * \see Project Source Code: https://github.com/ef-gy/cxxhttp
 * \see Licence Terms: https://github.com/ef-gy/cxxhttp/blob/master/COPYING
 */

#if !defined(CXXHTTP_VERSION_H)
#define CXXHTTP_VERSION_H

#include <asio/version.hpp>
#include <ef.gy/version.h>
#include <string>

namespace cxxhttp {
static const unsigned int version = 1;

static const std::string identifier =
    "cxxhttp/" + std::to_string(version) + " asio/" +
    std::to_string(ASIO_VERSION) + " libefgy/" + std::to_string(efgy::version);
}

#endif

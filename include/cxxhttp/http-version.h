/* HTTP version number handling and type.
 *
 * Defines a type for an HTTP version, and code for parsing and representing
 * that.
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
#if !defined(CXXHTTP_HTTP_VERSION_H)
#define CXXHTTP_HTTP_VERSION_H

#include <array>
#include <string>

namespace cxxhttp {
namespace http {
/* HTTP protocol version and assorted functions.
 *
 * For parsing a version number, storing it and turning it back into a string.
 */
class version : public std::array<int, 2> {
 public:
  /* Create an empty version object.
   *
   * The version this creates is for HTTP/0.0, which is obviously invalid.
   */
  version(void) : std::array<int, 2>({{0, 0}}) {}

  /* Create a version object from two integers.
   * @major Major version.
   * @minor Minor version.
   *
   * Constructs a new HTTP version object using the numbers specified in the
   * parameters.
   */
  version(int major, int minor) : std::array<int, 2>({{major, minor}}) {}

  /* Create a version object from two strings.
   * @major Major version to parse.
   * @minor Minor version to parse.
   *
   * Constructs a new HTTP version object using the numbers specified in the
   * parameters. The parameters must only contain strings, otherwise things will
   * get weird and blow up. Probably.
   */
  version(const std::string &major, const std::string &minor)
      : std::array<int, 2>({{std::stoi(major), std::stoi(minor)}}) {}

  /* Report if a version is valid.
   *
   * The earliest version we consider valid is HTTP/0.9.
   *
   * @return `true` for anything past HTTP/0.9, `false` otherwise.
   */
  bool valid(void) const {
    static const version minVersion{0, 9};
    return (*this) >= minVersion;
  }

  /* Create an HTTP version string.
   *
   * Turns a parsed version back into a version string, for use in the HTTP
   * protocol over the wire.
   */
  operator std::string(void) const {
    return "HTTP/" + std::to_string((*this)[0]) + "." +
           std::to_string((*this)[1]);
  }
};
}  // namespace http
}  // namespace cxxhttp

#endif

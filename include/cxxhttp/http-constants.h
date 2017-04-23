/* HTTP protocol constants.
 *
 * Contains constants used in the HTTP protocol.
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
#if !defined(CXXHTTP_HTTP_CONSTANTS_H)
#define CXXHTTP_HTTP_CONSTANTS_H

#include <map>
#include <set>
#include <string>

namespace cxxhttp {
namespace http {
/* Known HTTP Status codes.
 *
 * See also:
 * * https://www.w3.org/Protocols/rfc2616/rfc2616-sec10.html#sec10
 * * https://tools.ietf.org/html/rfc7725
 */
static const std::map<unsigned, const char *> status{
    // 1xx - Informational
    {100, "Continue"},
    {101, "Switching Protocols"},
    // 2xx - Successful
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    // 3xx - Redirection
    {300, "Multiple Choices"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {307, "Temporary Redirect"},
    // 4xx - Client Error
    {400, "Client Error"},
    {401, "Unauthorised"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Request Entity Too Large"},
    {414, "Request-URI Too Long"},
    {415, "Unsupported Media Type"},
    {416, "Requested Range Not Satisfiable"},
    {417, "Expectation Failed"},
    {451, "Unavailable For Legal Reasons"},
    // 5xx - Server Error
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"},
};

static const std::set<std::string> method{
    "OPTIONS", "GET", "HEAD", "POST", "PUT", "DELETE", "TRACE", "CONNECT",
};

static const std::set<std::string> non405method{
    "OPTIONS", "TRACE",
};
}
}

#endif

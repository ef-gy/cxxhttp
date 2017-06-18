#<cldoc:cxxhttp>

Main Namespace.

All code in cxxhttp is confined to a single namespace, aptly named cxxhttp. In
addition to that, all the library header files are also contained in a cxxhttp/
directory, which should avoid any clashes in names in your own programme.

#<cldoc:cxxhttp::http>

HTTP handling.

Contains an HTTP server and templates for session management and processing by
user code.

#<cldoc:cxxhttp::http::grammar>

HTTP grammar fragments.

Contains grammar fragments used when parsing HTTP messages, mostly in the form
of regular expressions.

#<cldoc:cxxhttp::http::stdio>

HTTP-over-STDIO

Implements HTTP over STDIN and STDOUT, which is useful for, say, debugging or
for running your server behind (x)inetd.

#<cldoc:cxxhttp::httpd>

Servlet-based HTTP server.

A simple HTTP server that is based on a global, per-transport list of servlets.
There's also a few sample servlets in `include/cxxhttp/httpd-(...).h`, for
features like OPTIONS and TRACE queries.

You really want to use this and not the raw http.h implementation.

#<cldoc:cxxhttp::httpd::cli>

HTTP server command line arguments.

These arguments allow initialising an HTTP server on TCP and UNIX sockets. The
namespace contains appropriate specifications, along with the functions to set
everything up.

#<cldoc:cxxhttp::httpd::options>

HTTP OPTIONS implementation.

Include the `httpd-options.h` header to enable this servlet.

OPTIONS allows querying valid actions on any given resource, as well as the
special `*` resource, which is for server-wide capabilities. This particular
implementation will match the resource against all servlets for the given
resource, show the alllowed methods and a markdown rendition of the available
resource handlers.

#<cldoc:cxxhttp::httpd::trace>

HTTP TRACE implementation.

There's certain security concerns surrounding TRACE, but it's still a useful
debugging tool. Include the `httpd-trace.h` header if you feel like enabling
this servlet.

#<cldoc:cxxhttp::httpd::usage>

HTTP servlet usage hints.

Code in this namespace provides usage summaries for the available servlets, by
pointing out their method and resource signatures.

#<cldoc:cxxhttp::net>

Networking code.

Contains templates that deal with networking in one way or another. This code
is based on asio.hpp, an asynchronous I/O library for C++. The makefile knows
how to download this header-only library in case it's not installed.

#<cldoc:cxxhttp::transport>

Supported transport sockets.

asio.hpp supports several types of sockets, but the only ones we support are
the TCP and UNIX ones, or `asio::ip::tcp` and `asio::local::stream_protocol`.
STDIO support might happen in the future, however.

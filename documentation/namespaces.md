#<cldoc:cxxhttp>

Main Namespace.

All code in cxxhttp is confined to a single namespace, aptly named cxxhttp. In
addition to that, all the library header files are also contained in a cxxhttp/
directory, which should avoid any clashes in names in your own programme.

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

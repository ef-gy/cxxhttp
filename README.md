# CXXHTTP

A C++ library implementing an asynchronous HTTP server and client.

To clone this library, make sure you also clone the submodules. The --recursive
flag should do this for you:

    git clone --recursive https://github.com/ef-gy/cxxhttp.git

## Features

This library implements HTTP/1.1 for clients as well as servers. It's fairly
minimalistic, yet there's a few nifty parts that wouldn't strictly have been
necessary.

Some of these are:

* HTTP Content Negotation
* HTTP over TCP, UNIX sockets and STDIO
* CLI help screen with servlet summaries
* Optional OPTIONS implementation, with a markdown body that shows the supported
  methods and the relevant location regex
* Optional TRACE implementation
* Basic 100-continue flow
* Basic request validation
* Fallback HEAD handler

I believe the STDIO feature is quite unique, as is the excellent test coverage
of the library, for both the client and the server code.

### Non-Features

Some features didn't make it into the library for various reasons - mostly to
keep it small. Some of these are:

* HTTP 'chunked' Transfer Encoding - or any non-identity encoding, really
* HTTP Date headers, or any other timekeeping-related code
* Query string parsing - use REST API style location strings instead
* Logging - though there are internal flags and counters, which e.g. the
  Prometheus client library based on this makes use of
* Any form of SSL/TLS support - use a frontend server to provide this for you

Some of the omissions technically make this non-conforming to the HTTP/1.1
standard, but seeing as how the primary purpose of this library is to sit behind
an nginx (or similar) HTTP web server, this shouldn't be an issue in practice.

## Compiler Requirements

You must have a GNU make and a C++ compiler that supports C++14. On most
modern-ish Linux distributions that's pretty easy: just update your PMS and
install clang++ or g++. Preferrably clang++. No reason.

### OSX

On OSX, you'll be fine if you just install a current version of XCode.

### Ubuntu (LTS)

On Ubuntu, you're in for a bit of a rough ride. The only reliable way I've found
of getting things to compile, at least on 14.04, is to install this PPA and use
g++ 4.9:

    sudo add-apt-repository ppa:ubuntu-toolchain-r/test
    sudo apt-get update
    sudo apt-get install g++-4.9

To compile things, add CXX=g++-4.9 to your make commands, like this:

    make test CXX=g++-4.9

This may not be necessary if you haven't installed clang++ and if there's no
other versions of g++ installed. clang++ doesn't work on Ubuntu in c++14 mode,
because libc++ hasn't been updated past an ancient SVN snapshot.

### Debian/Raspbian

On Debian and Raspbian it should be sufficient to just update your package
sources and install clang++ and libc++:

    apt-get update
    apt-get install clang++ libc++1 libc++-dev

## Test suite

The library has a test suite, which you can run like this:

    make test

If this fails, something is wrong. Please open an issue at GitHub with the log:
https://github.com/ef-gy/cxxhttp/issues

## Getting Started

This library uses the ASIO asynchronous I/O headers as well as libefgy. If you
are starting a new project, it's probably easiest to use the build headers from
libefgy and git submodules to pull in the dependencies.

The following set of commands ought to get you started:

    PROJECT=funtastic-example
    git init ${PROJECT}
    cd ${PROJECT}
    git submodule add https://github.com/ef-gy/libefgy.git dependencies/libefgy
    git submodule add https://github.com/chriskohlhoff/asio.git dependencies/asio
    git submodule add https://github.com/ef-gy/cxxhttp.git dependencies/cxxhttp
    mkdir include
    mkdir include/${PROJECT}
    mkdir src
    ln -s ../dependencies/asio/asio/include/asio include/asio
    ln -s ../dependencies/asio/asio/include/asio.hpp include/asio.hpp
    ln -s ../dependencies/cxxhttp/include/cxxhttp include/cxxhttp
    ln -s ../dependencies/libefgy/include/ef.gy include/ef.gy

A trivial makefile to get you started would look like this:

    -include ef.gy/base.mk include/ef.gy/base.mk
    NAME:=funtastic-example

A very trivial sample server would be this (see src/server.cpp):

    #define ASIO_DISABLE_THREADS
    #include <cxxhttp/httpd.h>
    using namespace cxxhttp;

    static void hello(http::sessionData &session, std::smatch &) {
      session.reply(200, "Hello World!");
    }

    static http::servlet servlet("/", ::hello);

    int main(int argc, char *argv[]) { return cxxhttp::main(argc, argv); }

If you saved this as src/example.cpp, then you could run it like this:

    make example
    ./example http:localhost:8080

This will run the server and to see what it does, use:

    curl -i http://localhost:8080/

Which should print the "Hello World!" from above.

You can run the ./example programme without arguments to see all the available
command line options. In particular, there is "http:unix:...", which lets you
run your server on a UNIX socket.

See src/server.cpp for additional commentary, and the
include/cxxhttp/httpd-....h headers, which implement additional common features
that web servers tend to have.

### I'm lazy, can I clone this example from somewhere?

Sure, try this:

    git clone --recursive https://github.com/ef-gy/cxxhttp-example.git

Note the --recursive - that's because this is using submodules.

## Usage

You really should be running this behind a real web servers, for security and
such. Look into nginx for that, which is really good at forward-proxying.

## Updating to a newer version of the libraries

The command you're looking for is built into git:

    git submodule update --recursive --remote

Don't forget to commit the new state of the dependencies/ directory, if any of
the libraries had updates.

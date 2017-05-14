# CXXHTTP

[![Build Status](https://travis-ci.org/ef-gy/cxxhttp.svg?branch=master)](https://travis-ci.org/ef-gy/cxxhttp)
[![Coverage Status](https://coveralls.io/repos/github/ef-gy/cxxhttp/badge.svg?branch=master)](https://coveralls.io/github/ef-gy/cxxhttp?branch=master)

A C++ library implementing an asynchronous HTTP server and client.

To clone this library, make sure you also clone the submodules. The --recursive
flag should do this for you:

    git clone --recursive https://github.com/ef-gy/cxxhttp.git

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

    template <class transport>
    static void hello(typename http::server<transport>::session &session, std::smatch &) {
      session.reply(200, "Hello World!");
    }

    namespace tcp {
      static httpd::servlet<transport::tcp> hello("/", ::hello<transport::tcp>);
    }

    namespace unix {
      static httpd::servlet<transport::unix> hello("/", ::hello<transport::unix>);
    }

    int main(int argc, char *argv[]) { return cxxhttp::main(argc, argv); }

If you saved this as src/example.cpp, then you could run it like this:

    make example
    ./example http:localhost:8080

This will run the server and to see what it does, use:

    curl -i http://localhost:8080/

Which should print the "Hello World!" from above, and you'll notice that your
example server printed a "GET /" access log line.

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

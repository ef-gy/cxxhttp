# CXXHTTP
[![Build Status](https://travis-ci.org/ef-gy/cxxhttp.svg?branch=master)](https://travis-ci.org/ef-gy/cxxhttp)

A C++ library implementing an asynchronous HTTP server and client.

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
    static bool hello(typename net::http::server<transport>::session &session, std::smatch &) {
      session.reply(200, "Hello World!");
      return true;
    }

    namespace tcp {
      using asio::ip::tcp;
      static httpd::servlet<tcp> hello("/", ::hello<tcp>);
    }

    namespace unix {
      using asio::local::stream_protocol;
      static httpd::servlet<stream_protocol> hello("/", ::hello<stream_protocol>);
    }

    int main(int argc, char *argv[]) { return io::main(argc, argv); }

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

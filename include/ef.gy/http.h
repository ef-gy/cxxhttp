/**\file
 * \brief Boost::ASIO HTTP Server
 *
 * An asynchornous HTTP server implementation using Boost::ASIO and
 * Boost::Regex. You will need to have Boost installed and available when
 * using this header.
 *
 * \copyright
 * Copyright (c) 2012-2015, ef.gy Project Members
 * \copyright
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * \copyright
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * \copyright
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * \see Project Documentation: http://ef.gy/documentation/libefgy
 * \see Project Source Code: https://github.com/ef-gy/libefgy
 */

#if !defined(EF_GY_HTTP_H)
#define EF_GY_HTTP_H

#include <map>
#include <string>
#include <sstream>

#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/regex.hpp>

#include <boost/algorithm/string.hpp>

#include <regex>

namespace efgy {
/**\brief Networking code
 *
 * Contains templates that deal with networking in one way or another.
 * Currently only contains an HTTP server.
 */
namespace net {
/**\brief HTTP handling
 *
 * Contains an HTTP server and templates for session management and
 * processing by user code.
 */
namespace http {
/**\brief HTTP state classes
 *
 * Contains state classes which are used to keep track of the data
 * that a request processor functor needs to respond to a client's
 * HTTP request.
 */
namespace state {
/**\brief Stub state class
 *
 * This is the default state class, which contains no data at
 * all. Many kinds of requests do not need additional data to
 * process requests, and they'll be perfectly fine with this
 * state class.
 */
class stub {
public:
  /**\brief Constructor
   *
   * Discards the argument passed to it and does nothing.
   */
  stub(void *) {}
};
};

template <typename requestProcessor, typename stateClass = state::stub,
          std::size_t maxContentLength = (1024 * 1024 * 12)>
class server;

/**\brief Session wrapper
 *
 * Used by the server to keep track of all the data associated with
 * a single, asynchronous client connection.
 *
 * \tparam requestProcessor The functor class to handle requests.
 * \tparam stateClass       Data class needed to keep track of the
 *                          resources used by the requestProcessor.
 * \tparam maxContentLength Maximum size of incoming body data.
 */
template <typename requestProcessor, typename stateClass = state::stub,
          std::size_t maxContentLength = (1024 * 1024 * 12)>
class session {
protected:
  /**\brief HTTP request parser status
   *
   * Contains the current status of the request parser for the
   * current session.
   */
  enum {
    stRequest, /**< Request received. */
    stHeader,
        /**< Currently parsing the
         *   request header.
         */
    stContent,
        /**< Currently parsing the
         *   request body.
         */
    stProcessing,
        /**< Currently processing the
         *   request.
         */
    stErrorContentTooLarge,
        /**< Error: Content-Length is
         *   greater than
         *   maxContentLength
         */
    stShutdown /**< Will shut down the
                *   connection now.
                */
  } status;

  /**\brief Case-insensitive comparison functor
   *
   * A simple functor used by the attribute map to compare
   * strings without caring for the letter case.
   */
  class caseInsensitiveLT
      : private std::binary_function<std::string, std::string, bool> {
  public:
    /**\brief Case-insensitive string comparison
     *
     * Compares two strings case-insensitively.
     *
     * \param[in] a The first of the two strings to
     *              compare.
     * \param[in] b The second of the two strings to
     *              compare.
     *
     * \returns 'true' if the first string is less than
     *          the second.
     */
    bool operator()(const std::string &a, const std::string &b) const {
      return lexicographical_compare(a, b, boost::is_iless());
    }

  };

public:
  /**\brief Stream socket
   *
   * This is the asynchronous I/O socket that is used to
   * communicate with the client.
   */
  boost::asio::local::stream_protocol::socket socket;

  /**\brief The request's HTTP method
   *
   * Contains the HTTP method that was used in the request,
   * e.g. GET, HEAD, POST, PUT, etc.
   */
  std::string method;

  /**\brief Requested HTTP resource location
   *
   * This is the location that was requested, e.g. / or
   * /fortune or something like that.
   */
  std::string resource;

  /**\brief HTTP request headers
   *
   * Contains all the headers that were sent along with the
   * request. Things like Host: or DNT:. Uses a special
   * case-insensitive sorting function for the map, so that
   * you can query the contents without having to know the
   * case of the headers as they were sent.
   */
  std::map<std::string, std::string, caseInsensitiveLT> header;

  /**\brief HTTP request body
   *
   * Contains the request body, if the request contained one.
   */
  std::string content;

  /**\brief Auxiliary state
   *
   * This is a reference to a state object, if one was passed
   * to the constructor. Only useful for the user code to keep
   * track of programme state - such as database connections -
   * and completely ignored by the HTTP wrapper.
   */
  stateClass *state;

  /**\brief Construct with I/O service
   *
   * Constructs a session with the given asynchronous I/O
   * service.
   */
  session(boost::asio::io_service &pIOService)
      : socket(pIOService), status(stRequest), input() {}

  /**\brief Destructor
   *
   * Closes the socket, cancels all remaining requests and
   * sets the status to stShutdown.
   */
  ~session(void) {
    status = stShutdown;
    socket.shutdown(boost::asio::local::stream_protocol::socket::shutdown_both);
    socket.cancel();
    socket.close();
  }

  /**\brief Start processing
   *
   * Starts processing the incoming request. As a side-effect,
   * this method also sets the auxiliary state which is later
   * used by the request processor.
   *
   * \param[in] state The auxiliary state for the request
   *                  processor.
   */
  void start(stateClass *state) {
    this->state = state;
    read();
  }

  /**\brief Send reply
   *
   * Used by the processing code once it is known how to
   * answer the request contained in this object. The code
   * will automatically add a correct Content-Length header,
   * but further headers may be added using the free-form
   * header parameter. Headers are not checked for validity.
   *
   * \note The code will always reply with an HTTP/1.1 reply,
   *       regardless of the version in the request. If this
   *       is a concern for you, put the server behind an
   *       an nginx instance, which should fix up the output
   *       as necessary.
   *
   * \param[in] status The status to return.
   * \param[in] header Any additional headers to be sent.
   * \param[in] body   The response body to send back to the
   *                   client.
   */
  void reply(int status, const std::string &header, const std::string &body) {
    std::stringstream reply;
    reply << "HTTP/1.1 " << status << " NA\r\nContent-Length: " << body.length()
          << "\r\n" + header + "\r\n" + body;

    if (status < 400) {
      boost::asio::async_write(socket, boost::asio::buffer(reply.str()),
                               boost::bind(&session::handleWrite, this,
                                           boost::asio::placeholders::error));
    } else {
      boost::asio::async_write(socket, boost::asio::buffer(reply.str()),
                               boost::bind(&session::handleWriteClose, this,
                                           boost::asio::placeholders::error));
    }
  }

  /**\brief Send reply without custom headers
   *
   * Wraps around the 3-parameter reply() function, so that
   * you don't have to specify an empty header parameter if
   * you don't intend to set custom headers.
   *
   * \param[in] status The status to return.
   * \param[in] body   The response body to send back to the
   *                   client.
   */
  void reply(int status, const std::string &body) { reply(status, "", body); }

protected:
  /**\brief Read more data
   *
   * Called by ASIO to indicate that new data has been read
   * and can now be processed.
   *
   * The actual processing for the header is done with a set
   * of regexen, which greatly simplifies the header parsing.
   *
   * \param[in] error             Current error state.
   * \param[in] bytes_transferred The amount of data that has
   *                              been read.
   */
  void handleRead(const boost::system::error_code &error,
                  size_t bytes_transferred) {
    if (status == stShutdown) {
      return;
    }

    if (!error) {
      static const std::regex req(
          "(\\w+)\\s+([\\w\\d%/.:;()+-]+)\\s+HTTP/1.[01]\\s*");
      static const std::regex mime("([\\w-]+):\\s*(.*)\\s*");
      static const std::regex mimeContinued("[ \t]\\s*(.*)\\s*");

      std::istream is(&input);
      std::string s;

      std::smatch matches;

      switch (status) {
      case stRequest:
      case stHeader:
        std::getline(is, s);
        break;
      case stContent:
        s = std::string(contentLength, '\0');
        is.read(&s[0], contentLength);
        break;
      case stProcessing:
      case stErrorContentTooLarge:
      case stShutdown:
        break;
      }

      switch (status) {
      case stRequest:
        if (std::regex_match(s, matches, req)) {
          method = matches[1];
          resource = matches[2];

          header = std::map<std::string, std::string, caseInsensitiveLT>();
          status = stHeader;
        }
        break;

      case stHeader:
        if ((s == "\r") || (s == "")) {
          try {
            contentLength =
                std::atoi(std::string(header["Content-Length"]).c_str());
          }
          catch (...) {
            contentLength = 0;
          }

          if (contentLength > maxContentLength) {
            status = stErrorContentTooLarge;
            reply(400, "Request body too large");
          } else {
            status = stContent;
          }
        } else if (std::regex_match(s, matches, mimeContinued)) {
          header[lastHeader] += "," + std::string(matches[1]);
        } else if (std::regex_match(s, matches, mime)) {
          lastHeader = matches[1];
          header[matches[1]] = matches[2];
        }

        break;

      case stContent:
        content = s;
        status = stProcessing;

        /* processing the request takes places here */
        {
          requestProcessor rp;
          rp(*this);
        }

        break;

      case stProcessing:
      case stErrorContentTooLarge:
      case stShutdown:
        break;
      }

      switch (status) {
      case stRequest:
      case stHeader:
      case stContent:
        read();
      case stProcessing:
      case stErrorContentTooLarge:
      case stShutdown:
        break;
      }
    } else {
      delete this;
    }
  }

  /**\brief Asynchronouse write handler
   *
   * Decides whether or not things need to be written to the
   * stream, or if things need to be read instead.
   *
   * Automatically deletes the object on errors - which also
   * closes the connection automagically.
   *
   * \param[in] error Current error state.
   */
  void handleWrite(const boost::system::error_code &error) {
    if (status == stShutdown) {
      return;
    }

    if (!error) {
      if (status == stProcessing) {
        status = stRequest;
        read();
      }
    } else {
      delete this;
    }
  }

  /**\brief Asynchronouse write and close handler
   *
   * Decides whether or not things need to be written to the
   * stream, or if things need to be read instead.
   *
   * Automatically deletes the object on errors - which also
   * closes the connection automagically.
   *
   * \param[in] error Current error state.
   */
  void handleWriteClose(const boost::system::error_code &error) {
    if (status == stShutdown) {
      return;
    }

    delete this;
  }

  /**\brief Asynchronouse read handler
   *
   * Decides if things need to be read in and - if so - what
   * needs to be read.
   *
   * Automatically deletes the object on errors - which also
   * closes the connection automagically.
   */
  void read(void) {
    switch (status) {
    case stRequest:
    case stHeader:
      boost::asio::async_read_until(
          socket, input, "\n",
          boost::bind(&session::handleRead, this,
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
      break;
    case stContent:
      boost::asio::async_read(
          socket, input,
          boost::bind(&session::contentReadP, this,
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred),
          boost::bind(&session::handleRead, this,
                      boost::asio::placeholders::error,
                      boost::asio::placeholders::bytes_transferred));
      break;
    case stProcessing:
    case stErrorContentTooLarge:
    case stShutdown:
      break;
    }
  }

  /**\brief Predicate: has the request been fully parsed?
   *
   * Determines whether a request has fully been parsed and if
   * the connection can safely be severed. Will be nonzero if
   * an error occurred or if there is still data to be read.
   *
   * \param[in] error             Current error context.
   * \param[in] bytes_transferred How much data has been read.
   *
   * \returns 0 when the request has been fully parsed and the
   *          connection can now be dropped.
   */
  std::size_t contentReadP(const boost::system::error_code &error,
                           std::size_t bytes_transferred) {
    return (bool(error) || (bytes_transferred >= contentLength))
               ? 0
               : (contentLength - bytes_transferred);
  }

  /**\brief Name of the last parsed header
   *
   * This is the name of the last header line that was parsed.
   * Used with multi-line headers.
   */
  std::string lastHeader;

  /**\brief Content length
   *
   * This is the value of the Content-Length header. Used when
   * parsing a request with a body.
   */
  std::size_t contentLength;

  /**\brief ASIO input stream buffer
   *
   * This is the stream buffer that the object is reading
   * from.
   */
  boost::asio::streambuf input;
};

/**\brief HTTP server wrapper
 *
 * Contains the code that accepts incoming HTTP requests and
 * dispatches asynchronous processing.
 *
 * \tparam requestProcessor The functor class to handle requests.
 * \tparam stateClass       Data class needed to keep track of the
 *                          resources used by the requestProcessor.
 * \tparam maxContentLength Maximum size of incoming body data.
 */
template <typename requestProcessor, typename stateClass,
          std::size_t maxContentLength>
class server {
public:
  /**\brief Initialise with IO service
   *
   * Default constructor which binds an IOService to a UNIX
   * socket. The socket is bound asynchronously.
   *
   * \param[out] pIOService IO service to use.
   * \param[in]  socket     UNIX socket to bind.
   * \param[in]  stateData  Data to pas to the state class.
   */
  server(boost::asio::io_service &pIOService, const char *socket,
         void *stateData = 0)
      : IOService(pIOService),
        acceptor(pIOService,
                 boost::asio::local::stream_protocol::endpoint(socket)),
        state(stateData) {
    startAccept();
  }

protected:
  /**\brief Accept the next incoming connection
   *
   * This function creates a new, blank session to handle the
   * next incoming request.
   */
  void startAccept(void) {
    session<requestProcessor, stateClass> *newSession =
        new session<requestProcessor, stateClass, maxContentLength>(IOService);
    acceptor.async_accept(newSession->socket,
                          boost::bind(&server::handleAccept, this, newSession,
                                      boost::asio::placeholders::error));
  }

  /**\brief Handle next incoming connection
   *
   * Called by Boost::ASIO when a new inbound connection has
   * been accepted; this will make the session parse the
   * incoming request and dispatch it to the request processor
   * specified as a template argument.
   *
   * \param[in] newSession The blank session object that was
   *                       created by startAccept().
   * \param[in] error      Describes any error condition that
   *                       may have occurred.
   */
  void handleAccept(
      session<requestProcessor, stateClass, maxContentLength> *newSession,
      const boost::system::error_code &error) {
    if (!error) {
      newSession->start(&state);
    } else {
      delete newSession;
    }

    startAccept();
  }

  /**\brief IO service
   *
   * Bound in the constructor to a Boost::ASIO IO service
   * which handles asynchronous connections.
   */
  boost::asio::io_service &IOService;

  /**\brief Socket acceptor
   *
   * This is the acceptor which has been bound to the socket
   * specified in the constructor.
   */
  boost::asio::local::stream_protocol::acceptor acceptor;

  /**\brief Server state instance
   *
   * Contains the global server state, which is passed to each
   * new session upon being created.
   */
  stateClass state;
};
};
};
};

#endif

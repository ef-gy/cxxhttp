/*
 * This file is part of the ef.gy project.
 * See the appropriate repository at http://ef.gy/.git for exact file
 * modification records.
*/

/*
 * Copyright (c) 2012-2013, ef.gy Project Members
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
*/

#if !defined(EF_GY_HTTP_H)
#define EF_GY_HTTP_H

#include <map>
#include <string>

#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/regex.hpp>

#include <boost/algorithm/string.hpp>

namespace efgy
{
    namespace net
    {
        namespace http
        {
            template<typename requestProcessor>
            class session
            {
                protected:
                    static const int maxContentLength = (1024 * 1024 * 12);
                    enum { stRequest, stHeader, stContent, stProcessing, stErrorContentTooLarge, stShutdown } status;

                    class caseInsensitiveLT : private std::binary_function<std::string, std::string, bool>
                    {
                        public:
                            bool operator() (const std::string &a, const std::string &b) const
                            {
                                return lexicographical_compare (a, b, boost::is_iless());
                            }

                    };

                public:
                    boost::asio::local::stream_protocol::socket socket;
                    std::string method;
                    std::string resource;
                    std::map<std::string,std::string,caseInsensitiveLT> header;
                    std::string content;

                    session (boost::asio::io_service &pIOService)
                        : socket(pIOService), status(stRequest), input()
                        {}

                    ~session (void)
                        {
                            status = stShutdown;
                            socket.shutdown(boost::asio::local::stream_protocol::socket::shutdown_both);
                            socket.cancel();
                            socket.close();
                        }

                    void start (void)
                    {
                        read();
                    }

                    void reply (int status, const std::string &header, const std::string &body)
                    {
                        char nbuf[20];
                        snprintf (nbuf, 20, "%i", status);
                        std::string reply("HTTP/1.1 ");

                        reply += std::string(nbuf) + " NA\r\nContent-Length: ";

                        snprintf (nbuf, 20, "%lu", body.length());

                        reply += std::string(nbuf) + "\r\n" + header + "\r\n" + body;

                        if (status < 400)
                        {
                            boost::asio::async_write
                                (socket,
                                 boost::asio::buffer(reply),
                                 boost::bind (&session::handle_write, this,
                                              boost::asio::placeholders::error));
                        }
                        else
                        {
                            boost::asio::async_write
                                (socket,
                                 boost::asio::buffer(reply),
                                 boost::bind (&session::handle_write_close, this,
                                              boost::asio::placeholders::error));
                        }
                    }

                    void reply (int status, const std::string &body)
                    {
                        reply (status, "", body);
                    }

                protected:
                    void handle_read(const boost::system::error_code &error, size_t bytes_transferred)
                    {
                        if (status == stShutdown)
                        {
                            return;
                        }

                        if (!error)
                        {
                            static const boost::regex req("(\\w+)\\s+([\\w\\d%/.:;()+-]+)\\s+HTTP/1.[01]\\s*");
                            static const boost::regex mime("([\\w-]+):\\s*(.*)\\s*");
                            static const boost::regex mimeContinued("[ \t]\\s*(.*)\\s*");

                            std::istream is(&input);
                            std::string s;

                            boost::smatch matches;

                            switch(status)
                            {
                                case stRequest:
                                case stHeader:
                                    std::getline(is,s);
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

                            switch(status)
                            {
                                case stRequest:
                                    if (boost::regex_match(s, matches, req))
                                    {
                                        method   = matches[1];
                                        resource = matches[2];

                                        header = std::map<std::string,std::string,caseInsensitiveLT>();
                                        status = stHeader;
                                    }
                                    break;

                                case stHeader:
                                    if ((s == "\r") || (s == ""))
                                    {
                                        try
                                        {
                                            contentLength = std::atoi(std::string(header["Content-Length"]).c_str());
                                        }
                                        catch(...)
                                        {
                                            contentLength = 0;
                                        }

                                        if (contentLength > maxContentLength)
                                        {
                                            status = stErrorContentTooLarge;
                                            reply (400, "Request body too large");
                                        }
                                        else
                                        {
                                            status = stContent;
                                        }
                                    }
                                    else if (boost::regex_match(s, matches, mimeContinued))
                                    {
                                        header[lastHeader] += "," + matches[1];
                                    }
                                    else if (boost::regex_match(s, matches, mime))
                                    {
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

                            switch(status)
                            {
                                case stRequest:
                                case stHeader:
                                case stContent:
                                    read();
                                case stProcessing:
                                case stErrorContentTooLarge:
                                case stShutdown:
                                    break;
                            }
                        }
                        else
                        {
                            delete this;
                        }
                    }

                    void handle_write(const boost::system::error_code &error)
                    {
                        if (status == stShutdown)
                        {
                            return;
                        }

                        if (!error)
                        {
                            if (status == stProcessing)
                            {
                                status = stRequest;
                                read();
                            }
                        }
                        else
                        {
                            delete this;
                        }
                    }

                    void handle_write_close(const boost::system::error_code &error)
                    {
                        if (status == stShutdown)
                        {
                            return;
                        }

                        delete this;
                    }

                    void read(void)
                    {
                        switch (status)
                        {
                            case stRequest:
                            case stHeader:
                                boost::asio::async_read_until
                                    (socket, input, "\n", 
                                     boost::bind(&session::handle_read, this,
                                                 boost::asio::placeholders::error,
                                                 boost::asio::placeholders::bytes_transferred));
                                break;
                            case stContent:
                                boost::asio::async_read
                                    (socket, input,
                                     boost::bind(&session::contentReadP, this,
                                                 boost::asio::placeholders::error,
                                                 boost::asio::placeholders::bytes_transferred),
                                     boost::bind(&session::handle_read, this,
                                                 boost::asio::placeholders::error,
                                                 boost::asio::placeholders::bytes_transferred));
                                break;
                            case stProcessing:
                            case stErrorContentTooLarge:
                            case stShutdown:
                                break;
                        }
                    }

                    std::size_t contentReadP (const boost::system::error_code& error, std::size_t bytes_transferred)
                    {
                        return (bool(error) || (bytes_transferred >= contentLength)) ? 0 : (contentLength - bytes_transferred);
                    }

                    std::string lastHeader;
                    std::size_t contentLength;

                    boost::asio::streambuf input;
            };

            template<typename requestProcessor>
            class server
            {
                public:
                    server(boost::asio::io_service &pIOService, const char *socket)
                        : IOService(pIOService),
                          acceptor_(pIOService, boost::asio::local::stream_protocol::endpoint(socket))
                        {
                            start_accept();
                        }

                protected:
                    void start_accept()
                    {
                        session<requestProcessor> *new_session = new session<requestProcessor>(IOService);
                        acceptor_.async_accept(new_session->socket,
                            boost::bind(&server::handle_accept, this, new_session,
                                        boost::asio::placeholders::error));
                    }

                    void handle_accept(session<requestProcessor> *new_session, const boost::system::error_code &error)
                    {
                        if (!error)
                        {
                            new_session->start();
                        }
                        else
                        {
                            delete new_session;
                        }

                        start_accept();
                    }

                    boost::asio::io_service &IOService;
                    boost::asio::local::stream_protocol::acceptor acceptor_;
            };
        };
    };
};

#endif

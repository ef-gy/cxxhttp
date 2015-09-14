/**\file
 * \ingroup example-programmes
 * \brief FaaS: Fortune as a Service
 *
 * Call it like this:
 * \code
 * $ ./fortuned http:localhost:8080
 * \endcode
 *
 * With localhost and 8080 being a host name and port of your choosing. Then,
 * while the programme is running.
 *
 * \copyright
 * Copyright (c) 2015, ef.gy Project Members
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
 * \see Project Documentation: https://ef.gy/documentation/libefgy
 * \see Project Source Code: https://github.com/ef-gy/libefgy
 */

#define ASIO_DISABLE_THREADS
#include <ef.gy/httpd.h>

#include <cstdlib>
#include <dirent.h>
#include <iostream>
#include <fstream>

using namespace efgy;
using namespace std;

map<string, string> fortuneData;

std::string replace(std::string subject, const std::string &search,
                    const std::string &replace) {
  size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos) {
    subject.replace(pos, search.length(), replace);
    pos += replace.length();
  }
  return subject;
}

class cookie {
public:
  enum encoding { ePlain, eROT13 } encoding;
  string file;
  size_t offset;
  size_t length;

  cookie(enum encoding pE, const string &pFile, size_t pOffset, size_t pLength)
      : encoding(pE), file(pFile), offset(pOffset), length(pLength) {}

  operator string(void) const {
    string r(fortuneData[file].data() + offset, length);

    if (encoding == eROT13) {
      for (size_t i = 0; i < r.size(); i++) {
        char c = r[i];

        switch (c) {
        case 'a':
        case 'b':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'g':
        case 'h':
        case 'i':
        case 'j':
        case 'k':
        case 'l':
        case 'm':
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case 'G':
        case 'H':
        case 'I':
        case 'J':
        case 'K':
        case 'L':
        case 'M':
          r[i] = c + 13;
          break;
        case 'n':
        case 'o':
        case 'p':
        case 'q':
        case 'r':
        case 's':
        case 't':
        case 'u':
        case 'v':
        case 'w':
        case 'x':
        case 'y':
        case 'z':
        case 'N':
        case 'O':
        case 'P':
        case 'Q':
        case 'R':
        case 'S':
        case 'T':
        case 'U':
        case 'V':
        case 'W':
        case 'X':
        case 'Y':
        case 'Z':
          r[i] = c - 13;
          break;
        default:
          break;
        }
      }
    }

    return r;
  }
};

vector<cookie> cookies;

static bool
prepareFortunes(const string &pInoffensive = "/usr/share/games/fortunes/",
                const string &pOffensive = "/usr/share/games/fortunes/off/") {
  static const regex dataFile(".*/[a-zA-Z-]+");
  smatch matches;

  for (unsigned int q = 0; q < 2; q++) {
    string dir = (q == 0) ? pInoffensive : pOffensive;
    bool doROT13 = (q == 1);

    DIR *d = opendir(dir.c_str());
    if (!d) {
      continue;
    }
    struct dirent *en;
    while ((en = readdir(d)) != 0) {
      string e = dir + en->d_name;

      if (regex_match(e, matches, dataFile)) {
        std::ifstream t(e);
        std::stringstream buffer;
        buffer << t.rdbuf();

        fortuneData[e] = buffer.str();

        const string &p = fortuneData[e];
        const char *data = p.c_str();
        size_t start = 0;
        enum { stN, stNL, stNLP } state = stN;

        for (size_t c = 0; c < p.size(); c++) {
          switch (data[c]) {
          case '\n':
            switch (state) {
            case stN:
              state = stNL;
              break;
            case stNLP:
              cookies.push_back(
                  cookie((doROT13 ? cookie::eROT13 : cookie::ePlain), e, start,
                         c - start - 1));
              start = c + 1;
            default:
              state = stN;
              break;
            }
            break;

          case '%':
            switch (state) {
            case stNL:
              state = stNLP;
              break;
            default:
              state = stN;
              break;
            }
            break;

          case '\r':
            break;

          default:
            state = stN;
            break;
          }
        }
      }
    }
    closedir(d);
  }

  return true;
}

template <class transport>
static bool fortune(typename net::http::server<transport>::session &session,
                    std::smatch &) {
  const int id = rand() % cookies.size();
  const cookie &c = cookies[id];
  char nbuf[20];
  snprintf(nbuf, 20, "%i", id);
  string sc = string(c);

  /* note: this escaping is not exactly efficient, but it's fairly simple
     and the strings are fairly short, so it shouldn't be much of an issue. */
  for (char i = 0; i < 0x20; i++) {
    if ((i == '\n') || (i == '\t')) {
      continue;
    }
    const char org[2] = {i, 0};
    const char rep[3] = {'^', (char)(('A' - 1) + i), 0};
    replace(sc, org, rep);
  }

  sc = "<![CDATA[" + sc + "]]>";

  session.reply(200, "Content-Type: text/xml; charset=utf-8\r\n",
                string("<?xml version='1.0' encoding='utf-8'?>"
                       "<fortune xmlns='http://ef.gy/2012/fortune' quoteID='") +
                    nbuf + "' sourceFile='" + c.file + "'>" + sc +
                    "</fortune>");

  return true;
}

namespace tcp {
using asio::ip::tcp;
static httpd::servlet<tcp> fortune("^/fortune$", ::fortune<tcp>);
static httpd::servlet<tcp> quit("^/quit$", httpd::quit<tcp>);
}

namespace unix {
using asio::local::stream_protocol;
static httpd::servlet<stream_protocol> fortune("^/fortune$",
                                               ::fortune<stream_protocol>);
static httpd::servlet<stream_protocol> quit("^/quit$",
                                            httpd::quit<stream_protocol>);
}

static cli::option
    count("count", [](std::smatch &m) -> bool {
                     std::cout << cookies.size() << " cookie(s) loaded\n";

                     return true;
                   },
          "prints the number of fortune cookies in the database.");

static cli::option print("print(:([0-9]+))?",
                         [](std::smatch &m) -> bool {
                           if (m[1] != "") {
                             std::stringstream s(m[2]);
                             std::size_t n;
                             s >> n;
                             if (n < cookies.size()) {
                               std::cout << std::string(cookies[n]);
                             } else {
                               return false;
                             }
                           } else {
                             std::cout << std::string(
                                 cookies[(rand() % cookies.size())]);
                           }

                           return true;
                         },
                         "print a fortune to stdout"
                         " - a numerical parameter selects a specific cookie.");

int main(int argc, char *argv[]) {
  prepareFortunes();

  srand(time(0));

  return io::main(argc, argv);
}

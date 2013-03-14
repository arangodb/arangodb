////////////////////////////////////////////////////////////////////////////////
/// @brief http request result
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SimpleHttpResult.h"
#include "Basics/StringUtils.h"

using namespace triagens::basics;
using namespace std;

namespace triagens {
  namespace httpclient {

////////////////////////////////////////////////////////////////////////////////
/// constructor and destructor
////////////////////////////////////////////////////////////////////////////////

    SimpleHttpResult::SimpleHttpResult () {
      clear();
    }

    SimpleHttpResult::~SimpleHttpResult () {
    }

////////////////////////////////////////////////////////////////////////////////
/// public methods
////////////////////////////////////////////////////////////////////////////////

    void SimpleHttpResult::clear () {
      _returnCode = 0;
      _returnMessage = "";
      _contentLength = 0;
      _chunked = false;
      _requestResultType = UNKNOWN;
      _headerFields.clear();
      _resultBody.clear();
    }

    stringstream& SimpleHttpResult::getBody () {
      return _resultBody;
    }

    string SimpleHttpResult::getResultTypeMessage () {
      switch (_requestResultType) {
        case (COMPLETE):
          return "No error.";
        case (COULD_NOT_CONNECT):
          return "Could not connect to server.";
        case (WRITE_ERROR):
          return "Error while writing to server.";
        case (READ_ERROR):
          return "Error while reading from server.";
      default:
        return "Unknown error.";
      }
    }

    void SimpleHttpResult::addHeaderField (std::string const& line) {
      string key;
      string value;

       size_t find = line.find(": ");

       if (find != string::npos) {
         key = line.substr(0, find);
         value = StringUtils::trim(line.substr(find + 2));
         addHeaderField(key, value);
         return;
       }

       find = line.find(" ");
       if (find != string::npos) {
         key = line.substr(0, find);
         if (find + 1 < line.length()) {
           value = StringUtils::trim(line.substr(find + 1));
         }
         addHeaderField(key, value);
       }
    }

    void SimpleHttpResult::addHeaderField (std::string const& key, std::string const& value) {
      string k = StringUtils::trim(StringUtils::tolower(key));

      if (k == "http/1.1" || k == "http/1.0") {
        if (value.length() > 2) {
          // we assume the status code is 3 chars long
          string code = value.substr(0, 3);
          setHttpReturnCode(atoi(code.c_str()));
          setHttpReturnMessage(value.substr(3 + 1));
        }
      }
      else if (k == "content-length") {
        setContentLength((size_t) StringUtils::int64(value.c_str()));
      }
      else if (k == "transfer-encoding") {
        if (StringUtils::tolower(value) == "chunked") {
          _chunked = true;
        }
      }

      _headerFields[k] = value;
    }

    const string SimpleHttpResult::getContentType (const bool partial) {
      map<string, string>::const_iterator find = _headerFields.find("content-type");
      if (find != _headerFields.end()) {
        // header found
        if (partial) {
          // return partial match before first semicolon
          size_t semicolon = find->second.find(";");
          if (semicolon != string::npos) {
            // partial match found
            return find->second.substr(0, semicolon);
          }
        }
        return find->second;
      }

      return "";
    }
  }
}

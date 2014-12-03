////////////////////////////////////////////////////////////////////////////////
/// @brief http request result
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
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

    SimpleHttpResult::SimpleHttpResult ()
      : _returnMessage(),
        _contentLength(0),
        _returnCode(0),
        _foundHeader(false),
        _hasContentLength(false),
        _chunked(false),
        _deflated(false),
        _resultBody(TRI_UNKNOWN_MEM_ZONE),
        _requestResultType(UNKNOWN) {
    }

    SimpleHttpResult::~SimpleHttpResult () {
    }

////////////////////////////////////////////////////////////////////////////////
/// public methods
////////////////////////////////////////////////////////////////////////////////

    void SimpleHttpResult::clear () {
      _returnMessage = "";
      _contentLength = 0;
      _returnCode = 0;
      _hasContentLength = false;
      _chunked = false;
      _deflated = false;
      _requestResultType = UNKNOWN;
      _headerFields.clear();
      _resultBody.clear();
    }

    StringBuffer& SimpleHttpResult::getBody () {
      return _resultBody;
    }

    string SimpleHttpResult::getResultTypeMessage () const {
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
    
    void SimpleHttpResult::addHeaderField (char const* line, size_t length) {
      auto find = static_cast<char const*>(memchr(static_cast<void const*>(line), ':', length));

      if (find == nullptr) {
        find = static_cast<char const*>(memchr(static_cast<void const*>(line), ' ', length));
      }

      if (find != nullptr) {
        size_t l = find - line;
        addHeaderField(line, l, find + 1, length - l - 1);
      }
    }

    void SimpleHttpResult::addHeaderField (char const* key, 
                                           size_t keyLength,
                                           char const* value,
                                           size_t valueLength) {
      // trim key
      char const* end = key + keyLength;

      while (key < end && 
             (*key == ' ' || *key == '\t')) {
        ++key;
        --keyLength;
      }

      // lower-case key
      std::string keyString(key, keyLength);
      StringUtils::tolowerInPlace(&keyString);

      // trim value
      end = value + valueLength;

      while (value < end && 
             (*value == ' ' || *value == '\t')) {
        ++value;
        --valueLength;
      }

      if (keyString[0] == 'h') {
        if (! _foundHeader &&
            (keyString == "http/1.1" || keyString == "http/1.0")) {
          if (valueLength > 2) {
            _foundHeader = true;

            // we assume the status code is 3 chars long
            if ((value[0] >= '0' && value[0] <= '9') &&
                (value[1] >= '0' && value[1] <= '9') &&
                (value[2] >= '0' && value[2] <= '9')) {
              // set response code
              setHttpReturnCode(100 * (value[0] - '0') + 10 * (value[1] - '0') + (value[2] - '0'));
            }
         
            if (valueLength >= 4) {
              setHttpReturnMessage(std::string(value + 4, valueLength - 4));
            }
          }
        }
      }

      else if (keyString[0] == 'c') {

        if (keyString.size() == strlen("content-length") &&
            keyString == "content-length") {
          setContentLength((size_t) StringUtils::int64(value, valueLength));
          std::cout << "Simple: Content-length found: "
                    << getContentLength() << std::endl;
        }
        else if (keyString.size() == strlen("content-encoding") &&
                 keyString == "content-encoding") {
          if (valueLength == strlen("deflate") &&
              (value[0] == 'd' || value[0] == 'D') &&
              (value[1] == 'e' || value[1] == 'E') &&
              (value[2] == 'f' || value[2] == 'F') &&
              (value[3] == 'l' || value[3] == 'L') &&
              (value[4] == 'a' || value[4] == 'A') &&
              (value[5] == 't' || value[5] == 'T') &&
              (value[6] == 'e' || value[6] == 'E')) {
            _deflated = true;
          }
        }
      }

      else if (keyString[0] == 't') {

        if (keyString.size() == strlen("transfer-encoding") &&
            keyString == "transfer-encoding") {
          if (valueLength == strlen("chunked") &&
              (value[0] == 'c' || value[0] == 'C') &&
              (value[1] == 'h' || value[1] == 'H') &&
              (value[2] == 'u' || value[2] == 'U') &&
              (value[3] == 'n' || value[3] == 'N') &&
              (value[4] == 'k' || value[4] == 'K') &&
              (value[5] == 'e' || value[5] == 'E') &&
              (value[6] == 'd' || value[6] == 'D')) {
            _chunked = true;
          }
        }
      }

      auto result = _headerFields.emplace(keyString, std::string(value, valueLength));
      if (! result.second) {
        // header already present
        _headerFields[keyString] = std::string(value, valueLength);
      }
    }

    std::string SimpleHttpResult::getHeaderField (std::string const& name, bool& found) const {
      auto find = _headerFields.find(name);

      if (find == _headerFields.end()) {
        found = false;
        return string("");
      }

      found = true;
      return (*find).second;
    }

    bool SimpleHttpResult::isJson () const {
      auto find = _headerFields.find("content-type");

      if (find == _headerFields.end()) {
        return false;
      }

      // header found
      // return partial match before first semicolon
      if (strncmp(find->second.c_str(), "application/json", strlen("application/json")) != 0) {
        return false;
      }

      char const* ptr = find->second.c_str() + strlen("application/json");
      return (*ptr == '\0' || *ptr == ';' || *ptr == ' ');
    }

  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

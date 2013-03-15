////////////////////////////////////////////////////////////////////////////////
/// @brief import helper
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

#include "ImportHelper.h"

#include <sstream>
#include <iomanip>

#include "Basics/StringUtils.h"
#include "BasicsC/files.h"
#include "BasicsC/json.h"
#include "BasicsC/strings.h"
#include "Rest/HttpRequest.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::rest;
using namespace std;

namespace triagens {
  namespace v8client {

////////////////////////////////////////////////////////////////////////////////
/// initialise step value for progress reports
////////////////////////////////////////////////////////////////////////////////

    const double ImportHelper::ProgressStep = 2.0;

////////////////////////////////////////////////////////////////////////////////
/// constructor and destructor
////////////////////////////////////////////////////////////////////////////////

    ImportHelper::ImportHelper (httpclient::SimpleHttpClient* _client, uint64_t maxUploadSize)
    : _client(_client),
      _maxUploadSize(maxUploadSize),
      _lineBuffer(TRI_UNKNOWN_MEM_ZONE),
      _outputBuffer(TRI_UNKNOWN_MEM_ZONE) {
      _quote = "\"";
      _separator = ",";
      _createCollection = false;
      _progress = false;
      regcomp(&_doubleRegex, "^[-+]?([0-9]+\\.?[0-9]*|\\.[0-9]+)([eE][-+]?[0-8]+)?$", REG_EXTENDED);
      regcomp(&_intRegex, "^[-+]?([0-9]+)$", REG_EXTENDED);
      _hasError = false;
    }

    ImportHelper::~ImportHelper () {
      regfree(&_doubleRegex);
      regfree(&_intRegex);
    }

////////////////////////////////////////////////////////////////////////////////
/// public functions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief imports a delmiited file
////////////////////////////////////////////////////////////////////////////////

    bool ImportHelper::importDelimited (const string& collectionName,
                                        const string& fileName,
                                        const DelimitedImportType typeImport) {
      _collectionName = collectionName;
      _firstLine = "";
      _numberLines = 0;
      _numberOk = 0;
      _numberError = 0;
      _outputBuffer.clear();
      _lineBuffer.clear();
      _errorMessage = "";
      _hasError = false;

      // read and convert
      int fd;
      int64_t totalLength;

      if (fileName == "-") {
        // we don't have a filesize
        totalLength = 0;
        fd = STDIN_FILENO;
      }
      else {
        // read filesize
        totalLength = TRI_SizeFile(fileName.c_str());
        fd = TRI_OPEN(fileName.c_str(), O_RDONLY);
      }

      if (fd < 0) {
        _errorMessage = TRI_LAST_ERROR_STR;
        return false;
      }

      // progress display control variables
      int64_t totalRead = 0;
      double nextProgress = ProgressStep;

      size_t separatorLength;
      char* separator = TRI_UnescapeUtf8StringZ(TRI_UNKNOWN_MEM_ZONE, _separator.c_str(), _separator.size(), &separatorLength);
      if (separator == NULL) {
        _errorMessage = "out of memory";
        return false;
      }

      TRI_csv_parser_t parser;

      TRI_InitCsvParser(&parser,
                        TRI_UNKNOWN_MEM_ZONE,
                        ProcessCsvBegin,
                        ProcessCsvAdd,
                        ProcessCsvEnd);

      TRI_SetSeparatorCsvParser(&parser, separator[0]);

      // in csv, we'll use the quote char if set
      // in tsv, we do not use the quote char
      if (typeImport == ImportHelper::CSV && _quote.size() > 0) {
        TRI_SetQuoteCsvParser(&parser, _quote[0], true);
      }
      else {
        TRI_SetQuoteCsvParser(&parser, '\0', false);
      }
      parser._dataAdd = this;
      _rowOffset = 0;
      _rowsRead  = 0;

      char buffer[32768];

      while (! _hasError) {
        ssize_t n = TRI_READ(fd, buffer, sizeof(buffer));

        if (n < 0) {
          TRI_Free(TRI_UNKNOWN_MEM_ZONE, separator);
          TRI_DestroyCsvParser(&parser);
          _errorMessage = TRI_LAST_ERROR_STR;
          return false;
        }
        else if (n == 0) {
          break;
        }

        totalRead += (int64_t) n;
        reportProgress(totalLength, totalRead, nextProgress);

        TRI_ParseCsvString2(&parser, buffer, n);
      }

      if (_outputBuffer.length() > 0) {
        sendCsvBuffer();
      }

      TRI_DestroyCsvParser(&parser);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, separator);

      if (fileName != "-") {
        TRI_CLOSE(fd);
      }

      _outputBuffer.clear();
      return !_hasError;
    }

    bool ImportHelper::importJson (const string& collectionName, const string& fileName) {
      _collectionName = collectionName;
      _firstLine = "";
      _numberLines = 0;
      _numberOk = 0;
      _numberError = 0;
      _outputBuffer.clear();
      _errorMessage = "";
      _hasError = false;

      // read and convert
      int fd;
      int64_t totalLength;

      if (fileName == "-") {
        // we don't have a filesize
        totalLength = 0;
        fd = STDIN_FILENO;
      }
      else {
        // read filesize
        totalLength = TRI_SizeFile(fileName.c_str());
        fd = TRI_OPEN(fileName.c_str(), O_RDONLY);
      }

      if (fd < 0) {
        _errorMessage = TRI_LAST_ERROR_STR;
        return false;
      }

      char buffer[32768];
      bool isArray = false;
      bool checkedFront = false;

      // progress display control variables
      int64_t totalRead = 0;
      double nextProgress = ProgressStep;

      while (! _hasError) {
        ssize_t n = TRI_READ(fd, buffer, sizeof(buffer));

        if (n < 0) {
          _errorMessage = TRI_LAST_ERROR_STR;
          return false;
        }
        else if (n == 0) {
          // we're done
          break;
        }

        if (! checkedFront) {
          // detect the import file format (single lines with individual JSON objects
          // or a JSON array with all documents)
          const string firstChar = StringUtils::lTrim(string(buffer, n), "\r\n\t\f\b ").substr(0, 1);
          isArray = (firstChar == "[");
          checkedFront = true;
        }

        _outputBuffer.appendText(buffer, n);

        totalRead += (int64_t) n;
        reportProgress(totalLength, totalRead, nextProgress);

        if (_outputBuffer.length() > _maxUploadSize) {
          if (isArray) {
            _errorMessage = "import file is too big.";
            return false;
          }

          // send all data before last '\n'
          const char* first = _outputBuffer.c_str();
          char* pos = (char*) memrchr(first, '\n', _outputBuffer.length());

          if (pos != 0) {
            size_t len = pos - first + 1;
            sendJsonBuffer(first, len, isArray);
            _outputBuffer.erase_front(len);
          }
        }
      }

      if (_outputBuffer.length() > 0) {
        sendJsonBuffer(_outputBuffer.c_str(), _outputBuffer.length(), isArray);
      }

      _numberLines = _numberError + _numberOk;

      if (fileName != "-") {
        TRI_CLOSE(fd);
      }

      _outputBuffer.clear();
      return ! _hasError;
    }


////////////////////////////////////////////////////////////////////////////////
/// private functions
////////////////////////////////////////////////////////////////////////////////

    void ImportHelper::reportProgress (const int64_t totalLength,
                                       const int64_t totalRead,
                                       double& nextProgress) {
      if (! _progress || totalLength == 0) {
        return;
      }

      double pct = 100.0 * ((double) totalRead / (double) totalLength);
      if (pct >= nextProgress) {
        LOGGER_INFO("processed " << totalRead << " bytes (" << std::fixed << std::setprecision(2) << pct << " %) of input file");
        nextProgress = pct + ProgressStep;
      }
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection-related URL part
////////////////////////////////////////////////////////////////////////////////

    string ImportHelper::getCollectionUrlPart () {
      string part("collection=" + StringUtils::urlEncode(_collectionName));

      if (_createCollection) {
        part += "&createCollection=yes";
      }

      return part;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief start a new csv line
////////////////////////////////////////////////////////////////////////////////

    void ImportHelper::ProcessCsvBegin (TRI_csv_parser_t* parser, size_t row) {
      ImportHelper* ih = reinterpret_cast<ImportHelper*> (parser->_dataAdd);

      if (ih) {
        ih->beginLine(row);
      }
    }

    void ImportHelper::beginLine(size_t row) {
      if (_lineBuffer.length() > 0) {
        // error
        ++_numberError;
        _lineBuffer.clear();
      }

      ++_numberLines;

      if (row > 0) {
        _lineBuffer.appendChar('\n');
      }
      _lineBuffer.appendChar('[');
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new CSV field
////////////////////////////////////////////////////////////////////////////////

    void ImportHelper::ProcessCsvAdd (TRI_csv_parser_t* parser, char const* field, size_t row, size_t column, bool escaped) {
      ImportHelper* ih = reinterpret_cast<ImportHelper*> (parser->_dataAdd);

      if (ih) {
        ih->addField(field, row, column, escaped);
      }
    }

    void ImportHelper::addField (char const* field, size_t row, size_t column, bool escaped) {
      if (column > 0) {
        _lineBuffer.appendChar(',');
      }

      if (row == 0) {
        // head line
        _lineBuffer.appendChar('"');
        _lineBuffer.appendText(StringUtils::escapeUnicode(field));
        _lineBuffer.appendChar('"');
      }
      else {
        if (escaped) {
          _lineBuffer.appendChar('"');
          _lineBuffer.appendText(StringUtils::escapeUnicode(field));
          _lineBuffer.appendChar('"');
        }
        else {
          string s(field);
          if (s.length() == 0) {
            // do nothing
            _lineBuffer.appendText("null");
          }
          else if ("true" == s || "false" == s) {
            _lineBuffer.appendText(field);
          }
          else {
            if (regexec(&_intRegex, s.c_str(), 0, 0, 0) == 0) {
              int64_t num = StringUtils::int64(s);
              _lineBuffer.appendInteger(num);
            }
            else if (regexec(&_doubleRegex, s.c_str(), 0, 0, 0) == 0) {
              double num = StringUtils::doubleDecimal(s);
              _lineBuffer.appendDecimal(num);
            }
            else {
              _lineBuffer.appendChar('"');
              _lineBuffer.appendText(StringUtils::escapeUnicode(field));
              _lineBuffer.appendChar('"');
            }
          }
        }
      }
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief ends a CSV line
////////////////////////////////////////////////////////////////////////////////

    void ImportHelper::ProcessCsvEnd (TRI_csv_parser_t* parser, char const* field, size_t row, size_t column, bool escaped) {
      ImportHelper* ih = reinterpret_cast<ImportHelper*> (parser->_dataAdd);

      if (ih) {
        ih->addLastField(field, row, column, escaped);
        ih->incRowsRead();
      }
    }

    void ImportHelper::addLastField (char const* field, size_t row, size_t column, bool escaped) {
      if (column == 0 && StringUtils::trim(field) == "") {
        // ignore empty line
        _lineBuffer.clear();
        return;
      }

      addField(field, row, column, escaped);

      _lineBuffer.appendChar(']');

      if (row == 0) {
        // save the first line
        _firstLine = _lineBuffer.c_str();
      }
      else if (row > 0 && _firstLine == "") {
        // error
        ++_numberError;
        _lineBuffer.clear();
        return;
      }

      // read a complete line

      if (_lineBuffer.length() > 0) {
        _outputBuffer.appendText(_lineBuffer);
        _lineBuffer.clear();
      }
      else {
        ++_numberError;
      }

      if (_outputBuffer.length() > _maxUploadSize) {
        sendCsvBuffer();
        _outputBuffer.appendText(_firstLine);
      }

    }


    void ImportHelper::sendCsvBuffer () {
      if (_hasError) {
        return;
      }

      map<string, string> headerFields;
      string url("/_api/import?" + getCollectionUrlPart() + "&line=" + StringUtils::itoa(_rowOffset));
      SimpleHttpResult* result = _client->request(HttpRequest::HTTP_REQUEST_POST, url, _outputBuffer.c_str(), _outputBuffer.length(), headerFields);

      handleResult(result);

      _outputBuffer.clear();
      _rowOffset = _rowsRead;
    }

    void ImportHelper::sendJsonBuffer (char const* str, size_t len, bool isArray) {
      if (_hasError) {
        return;
      }

      map<string, string> headerFields;
      SimpleHttpResult* result;
      if (isArray) {
        result = _client->request(HttpRequest::HTTP_REQUEST_POST, "/_api/import?type=array&" + getCollectionUrlPart(), str, len, headerFields);
      }
      else {
        result = _client->request(HttpRequest::HTTP_REQUEST_POST, "/_api/import?type=documents&" + getCollectionUrlPart(), str, len, headerFields);
      }

      handleResult(result);
    }

    void ImportHelper::handleResult (SimpleHttpResult* result) {
      if (! result) {
        return;
      }

      stringstream& r = result->getBody();

      TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, r.str().c_str());

      if (json) {
        // get the "error" flag. This returns a pointer, not a copy
        TRI_json_t* error = TRI_LookupArrayJson(json, "error");

        if (error) {
          if (error->_type == TRI_JSON_BOOLEAN && error->_value._boolean) {
            _hasError = true;

            // get the error message. This returns a pointer, not a copy
            TRI_json_t* errorMessage = TRI_LookupArrayJson(json, "errorMessage");
            if (errorMessage) {
              if (errorMessage->_type == TRI_JSON_STRING) {
                _errorMessage = string(errorMessage->_value._string.data, errorMessage->_value._string.length);
              }
            }
          }
        }

        TRI_json_t* importResult;

        // look up the "created" flag. This returns a pointer, not a copy
        importResult= TRI_LookupArrayJson(json, "created");
        if (importResult) {
          if (importResult->_type == TRI_JSON_NUMBER) {
            _numberOk += (size_t) importResult->_value._number;
          }
        }

        // look up the "errors" flag. This returns a pointer, not a copy
        importResult= TRI_LookupArrayJson(json, "errors");
        if (importResult) {
          if (importResult->_type == TRI_JSON_NUMBER) {
            _numberError += (size_t) importResult->_value._number;
          }
        }

        // this will free the json struct will a sub-elements
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }

      delete result;
    }

  }
}


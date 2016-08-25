////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "ImportHelper.h"

#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Logger/Logger.h"
#include "Basics/tri-strings.h"
#include "Basics/VelocyPackHelper.h"
#include "Rest/GeneralResponse.h"
#include "Rest/HttpRequest.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to determine if a field value is an integer
/// this function is here to avoid usage of regexes, which are too slow
////////////////////////////////////////////////////////////////////////////////

static bool IsInteger(char const* field, size_t fieldLength) {
  char const* end = field + fieldLength;

  if (*field == '+' || *field == '-') {
    ++field;
  }

  while (field < end) {
    if (*field < '0' || *field > '9') {
      return false;
    }
    ++field;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper function to determine if a field value maybe is a decimal
/// value. this function peeks into the first few bytes of the value only
/// this function is here to avoid usage of regexes, which are too slow
////////////////////////////////////////////////////////////////////////////////

static bool IsDecimal(char const* field, size_t fieldLength) {
  char const* ptr = field;
  char const* end = ptr + fieldLength;

  if (*ptr == '+' || *ptr == '-') {
    ++ptr;
  }

  bool nextMustBeNumber = false;

  while (ptr < end) {
    if (*ptr == '.') {
      if (nextMustBeNumber) {
        return false;
      }
      // expect a number after the .
      nextMustBeNumber = true;
    } else if (*ptr == 'e' || *ptr == 'E') {
      if (nextMustBeNumber) {
        return false;
      }
      // expect a number after the exponent
      nextMustBeNumber = true;

      ++ptr;
      if (ptr >= end) {
        return false;
      }
      // skip over optional + or -
      if (*ptr == '+' || *ptr == '-') {
        ++ptr;
      }
      // do not advance ptr anymore
      continue;
    } else if (*ptr >= '0' && *ptr <= '9') {
      // found a number
      nextMustBeNumber = false;
    } else {
      // something else
      return false;
    }

    ++ptr;
  }

  if (nextMustBeNumber) {
    return false;
  }

  return true;
}

namespace arangodb {
namespace import {

////////////////////////////////////////////////////////////////////////////////
/// initialize step value for progress reports
////////////////////////////////////////////////////////////////////////////////

double const ImportHelper::ProgressStep = 3.0;

////////////////////////////////////////////////////////////////////////////////
/// constructor and destructor
////////////////////////////////////////////////////////////////////////////////

ImportHelper::ImportHelper(httpclient::SimpleHttpClient* client,
                           uint64_t maxUploadSize)
    : _client(client),
      _maxUploadSize(maxUploadSize),
      _separator(","),
      _quote("\""),
      _createCollectionType("document"),
      _useBackslash(false),
      _convert(true),
      _createCollection(false),
      _overwrite(false),
      _progress(false),
      _firstChunk(true),
      _numberLines(0),
      _numberCreated(0),
      _numberErrors(0),
      _numberUpdated(0),
      _numberIgnored(0),
      _rowsRead(0),
      _rowOffset(0),
      _rowsToSkip(0),
      _onDuplicateAction("error"),
      _collectionName(),
      _lineBuffer(TRI_UNKNOWN_MEM_ZONE),
      _outputBuffer(TRI_UNKNOWN_MEM_ZONE) {
  _hasError = false;
}

ImportHelper::~ImportHelper() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief imports a delimited file
////////////////////////////////////////////////////////////////////////////////

bool ImportHelper::importDelimited(std::string const& collectionName,
                                   std::string const& fileName,
                                   DelimitedImportType typeImport) {
  _collectionName = collectionName;
  _firstLine = "";
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
  } else {
    // read filesize
    totalLength = TRI_SizeFile(fileName.c_str());
    fd = TRI_OPEN(fileName.c_str(), O_RDONLY | TRI_O_CLOEXEC);

    if (fd < 0) {
      _errorMessage = TRI_LAST_ERROR_STR;
      return false;
    }
  }

  // progress display control variables
  int64_t totalRead = 0;
  double nextProgress = ProgressStep;

  size_t separatorLength;
  char* separator =
      TRI_UnescapeUtf8String(TRI_UNKNOWN_MEM_ZONE, _separator.c_str(),
                             _separator.size(), &separatorLength, true);

  if (separator == nullptr) {
    if (fd != STDIN_FILENO) {
      TRI_CLOSE(fd);
    }

    _errorMessage = "out of memory";
    return false;
  }

  TRI_csv_parser_t parser;

  TRI_InitCsvParser(&parser, TRI_UNKNOWN_MEM_ZONE, ProcessCsvBegin,
                    ProcessCsvAdd, ProcessCsvEnd, nullptr);

  TRI_SetSeparatorCsvParser(&parser, separator[0]);
  TRI_UseBackslashCsvParser(&parser, _useBackslash);

  // in csv, we'll use the quote char if set
  // in tsv, we do not use the quote char
  if (typeImport == ImportHelper::CSV && _quote.size() > 0) {
    TRI_SetQuoteCsvParser(&parser, _quote[0], true);
  } else {
    TRI_SetQuoteCsvParser(&parser, '\0', false);
  }
  parser._dataAdd = this;
  _rowOffset = 0;
  _rowsRead = 0;

  char buffer[32768];

  while (!_hasError) {
    ssize_t n = TRI_READ(fd, buffer, sizeof(buffer));

    if (n < 0) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, separator);
      TRI_DestroyCsvParser(&parser);
      if (fd != STDIN_FILENO) {
        TRI_CLOSE(fd);
      }
      _errorMessage = TRI_LAST_ERROR_STR;
      return false;
    } else if (n == 0) {
      break;
    }

    totalRead += (int64_t)n;
    reportProgress(totalLength, totalRead, nextProgress);

    TRI_ParseCsvString(&parser, buffer, n);
  }

  if (_outputBuffer.length() > 0) {
    sendCsvBuffer();
  }

  TRI_DestroyCsvParser(&parser);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, separator);

  if (fd != STDIN_FILENO) {
    TRI_CLOSE(fd);
  }

  _outputBuffer.clear();
  return !_hasError;
}

bool ImportHelper::importJson(std::string const& collectionName,
                              std::string const& fileName) {
  _collectionName = collectionName;
  _firstLine = "";
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
  } else {
    // read filesize
    totalLength = TRI_SizeFile(fileName.c_str());
    fd = TRI_OPEN(fileName.c_str(), O_RDONLY | TRI_O_CLOEXEC);

    if (fd < 0) {
      _errorMessage = TRI_LAST_ERROR_STR;
      return false;
    }
  }

  bool isObject = false;
  bool checkedFront = false;

  // progress display control variables
  int64_t totalRead = 0;
  double nextProgress = ProgressStep;

  static int const BUFFER_SIZE = 32768;

  while (!_hasError) {
    // reserve enough room to read more data
    if (_outputBuffer.reserve(BUFFER_SIZE) == TRI_ERROR_OUT_OF_MEMORY) {
      _errorMessage = TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);

      if (fd != STDIN_FILENO) {
        TRI_CLOSE(fd);
      }
      return false;
    }

    // read directly into string buffer
    ssize_t n = TRI_READ(fd, _outputBuffer.end(), BUFFER_SIZE - 1);

    if (n < 0) {
      _errorMessage = TRI_LAST_ERROR_STR;
      if (fd != STDIN_FILENO) {
        TRI_CLOSE(fd);
      }
      return false;
    } else if (n == 0) {
      // we're done
      break;
    }

    // adjust size of the buffer by the size of the chunk we just read
    _outputBuffer.increaseLength(n);

    if (!checkedFront) {
      // detect the import file format (single lines with individual JSON
      // objects
      // or a JSON array with all documents)
      char const* p = _outputBuffer.begin();
      char const* e = _outputBuffer.end();

      while (p < e && (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t' ||
                       *p == '\f' || *p == '\b')) {
        ++p;
      }

      isObject = (*p == '[');
      checkedFront = true;
    }

    totalRead += (int64_t)n;
    reportProgress(totalLength, totalRead, nextProgress);

    if (_outputBuffer.length() > _maxUploadSize) {
      if (isObject) {
        if (fd != STDIN_FILENO) {
          TRI_CLOSE(fd);
        }
        _errorMessage =
            "import file is too big. please increase the value of --batch-size "
            "(currently " +
            StringUtils::itoa(_maxUploadSize) + ")";
        return false;
      }

      // send all data before last '\n'
      char const* first = _outputBuffer.c_str();
      char* pos = (char*)memrchr(first, '\n', _outputBuffer.length());

      if (pos != nullptr) {
        size_t len = pos - first + 1;
        sendJsonBuffer(first, len, isObject);
        _outputBuffer.erase_front(len);
      }
    }
  }

  if (_outputBuffer.length() > 0) {
    sendJsonBuffer(_outputBuffer.c_str(), _outputBuffer.length(), isObject);
  }

  if (fd != STDIN_FILENO) {
    TRI_CLOSE(fd);
  }

  // this is an approximation only. _numberLines is more meaningful for CSV
  // imports
  _numberLines =
      _numberErrors + _numberCreated + _numberIgnored + _numberUpdated;

  _outputBuffer.clear();
  return !_hasError;
}

////////////////////////////////////////////////////////////////////////////////
/// private functions
////////////////////////////////////////////////////////////////////////////////

void ImportHelper::reportProgress(int64_t totalLength, int64_t totalRead,
                                  double& nextProgress) {
  if (!_progress) {
    return;
  }

  if (totalLength == 0) {
    // length of input is unknown
    // in this case we cannot report the progress as a percentage
    // instead, report every 10 MB processed
    static int64_t nextProcessed = 10 * 1000 * 1000;

    if (totalRead >= nextProcessed) {
      LOG(INFO) << "processed " << totalRead << " bytes of input file";
      nextProcessed += 10 * 1000 * 1000;
    }
  } else {
    double pct = 100.0 * ((double)totalRead / (double)totalLength);

    if (pct >= nextProgress && totalLength >= 1024) {
      LOG(INFO) << "processed " << totalRead << " bytes (" << (int)nextProgress
                << "%) of input file";
      nextProgress = (double)((int)(pct + ProgressStep));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection-related URL part
////////////////////////////////////////////////////////////////////////////////

std::string ImportHelper::getCollectionUrlPart() const {
  return std::string("collection=" + StringUtils::urlEncode(_collectionName));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a new csv line
////////////////////////////////////////////////////////////////////////////////

void ImportHelper::ProcessCsvBegin(TRI_csv_parser_t* parser, size_t row) {
  static_cast<ImportHelper*>(parser->_dataAdd)->beginLine(row);
}

void ImportHelper::beginLine(size_t row) {
  if (_lineBuffer.length() > 0) {
    // error
    ++_numberErrors;
    _lineBuffer.clear();
  }

  ++_numberLines;

  if (row > 0 + _rowsToSkip) {
    _lineBuffer.appendChar('\n');
  }
  _lineBuffer.appendChar('[');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new CSV field
////////////////////////////////////////////////////////////////////////////////

void ImportHelper::ProcessCsvAdd(TRI_csv_parser_t* parser, char const* field,
                                 size_t fieldLength, size_t row, size_t column,
                                 bool escaped) {
  auto importHelper = static_cast<ImportHelper*>(parser->_dataAdd);

  if (importHelper->getRowsRead() < importHelper->getRowsToSkip()) {
    return;
  }
    
  importHelper->addField(field, fieldLength, row, column, escaped);
}

void ImportHelper::addField(char const* field, size_t fieldLength, size_t row,
                            size_t column, bool escaped) {
  if (column > 0) {
    _lineBuffer.appendChar(',');
  }

  if (row == 0 + _rowsToSkip || escaped) {
    // head line or escaped value
    _lineBuffer.appendJsonEncoded(field, fieldLength);
    return;
  }
    
  if (!_convert) {
    _lineBuffer.appendText(field, fieldLength);
    return;
  }

  if (*field == '\0') {
    // do nothing
    _lineBuffer.appendText(TRI_CHAR_LENGTH_PAIR("null"));
    return;
  }

  // check for literals null, false and true
  if (fieldLength == 4 &&
      (memcmp(field, "true", 4) == 0 || memcmp(field, "null", 4) == 0)) {
    _lineBuffer.appendText(field, fieldLength);
    return;
  } else if (fieldLength == 5 && memcmp(field, "false", 5) == 0) {
    _lineBuffer.appendText(field, fieldLength);
    return;
  }

  if (IsInteger(field, fieldLength)) {
    // integer value
    // conversion might fail with out-of-range error
    try {
      if (fieldLength > 8) {
        // long integer numbers might be problematic. check if we get out of
        // range
        (void) std::stoll(std::string(
            field,
            fieldLength));  // this will fail if the number cannot be converted
      }

      int64_t num = StringUtils::int64(field, fieldLength);
      _lineBuffer.appendInteger(num);
    } catch (...) {
      // conversion failed
      _lineBuffer.appendJsonEncoded(field, fieldLength);
    }
  } else if (IsDecimal(field, fieldLength)) {
    // double value
    // conversion might fail with out-of-range error
    try {
      std::string tmp(field, fieldLength);
      size_t pos = 0;
      double num = std::stod(tmp, &pos);
      if (pos == fieldLength) {
        bool failed = (num != num || num == HUGE_VAL || num == -HUGE_VAL);
        if (!failed) {
          _lineBuffer.appendDecimal(num);
          return;
        }
      }
      // NaN, +inf, -inf
      // fall-through to appending the number as a string
    } catch (...) {
      // conversion failed
      // fall-through to appending the number as a string
    }

    _lineBuffer.appendChar('"');
    _lineBuffer.appendText(field, fieldLength);
    _lineBuffer.appendChar('"');
  } else {
    _lineBuffer.appendJsonEncoded(field, fieldLength);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ends a CSV line
////////////////////////////////////////////////////////////////////////////////

void ImportHelper::ProcessCsvEnd(TRI_csv_parser_t* parser, char const* field,
                                 size_t fieldLength, size_t row, size_t column,
                                 bool escaped) {
  auto importHelper = static_cast<ImportHelper*>(parser->_dataAdd);
  
  if (importHelper->getRowsRead() < importHelper->getRowsToSkip()) {
    importHelper->incRowsRead();
    return;
  }
    
  importHelper->addLastField(field, fieldLength, row, column, escaped);
  importHelper->incRowsRead();
}

void ImportHelper::addLastField(char const* field, size_t fieldLength,
                                size_t row, size_t column, bool escaped) {
  if (column == 0 && *field == '\0') {
    // ignore empty line
    _lineBuffer.reset();
    return;
  }

  addField(field, fieldLength, row, column, escaped);

  _lineBuffer.appendChar(']');

  if (row == _rowsToSkip) {
    // save the first line
    _firstLine = _lineBuffer.c_str();
  } else if (row > _rowsToSkip && _firstLine.empty()) {
    // error
    ++_numberErrors;
    _lineBuffer.reset();
    return;
  }

  // read a complete line

  if (_lineBuffer.length() > 0) {
    _outputBuffer.appendText(_lineBuffer);
    _lineBuffer.reset();
  } else {
    ++_numberErrors;
  }

  if (_outputBuffer.length() > _maxUploadSize) {
    sendCsvBuffer();
    _outputBuffer.appendText(_firstLine);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if we must create the target collection, and create it if
/// required
////////////////////////////////////////////////////////////////////////////////
  
bool ImportHelper::checkCreateCollection() {
  if (!_firstChunk || !_createCollection) {
    return true;
  }
  
  std::string const url("/_api/collection");

  VPackBuilder builder;
  builder.openObject();
  builder.add("name", VPackValue(_collectionName));
  builder.add("type", VPackValue(_createCollectionType == "edge" ? 3 : 2));
  builder.close();

  std::string data = builder.slice().toJson();
  
  std::unordered_map<std::string, std::string> headerFields;
  std::unique_ptr<SimpleHttpResult> result(_client->request(
      rest::RequestType::POST, url, data.c_str(),
      data.size(), headerFields));

  if (result == nullptr) {
    return false;
  }

  auto code = static_cast<rest::ResponseCode>(result->getHttpReturnCode());
  if (code == rest::ResponseCode::CONFLICT ||
      code == rest::ResponseCode::OK ||
      code == rest::ResponseCode::CREATED ||
      code == rest::ResponseCode::ACCEPTED) {
    // collection already exists or was created successfully
    return true;
  }

  LOG(ERR) << "unable to create collection '" << _collectionName << "', server returned status code: " << static_cast<int>(code); 
  _hasError = true;
  return false;
}

void ImportHelper::sendCsvBuffer() {
  if (_hasError) {
    return;
  }
  
  if (!checkCreateCollection()) {
    return;
  }
    
  std::unordered_map<std::string, std::string> headerFields;
  std::string url("/_api/import?" + getCollectionUrlPart() + "&line=" +
                  StringUtils::itoa(_rowOffset) + "&details=true&onDuplicate=" +
                  StringUtils::urlEncode(_onDuplicateAction));

  if (!_fromCollectionPrefix.empty()) {
    url += "&fromPrefix=" + StringUtils::urlEncode(_fromCollectionPrefix);
  }
  if (!_toCollectionPrefix.empty()) {
    url += "&toPrefix=" + StringUtils::urlEncode(_toCollectionPrefix);
  }
  if (_firstChunk && _overwrite) {
    url += "&overwrite=true";
  }
  
  _firstChunk = false;

  std::unique_ptr<SimpleHttpResult> result(_client->request(
      rest::RequestType::POST, url, _outputBuffer.c_str(),
      _outputBuffer.length(), headerFields));

  handleResult(result.get());

  _outputBuffer.reset();
  _rowOffset = _rowsRead;
}

void ImportHelper::sendJsonBuffer(char const* str, size_t len, bool isObject) {
  if (_hasError) {
    return;
  }

  if (!checkCreateCollection()) {
    return;
  }

  // build target url
  std::string url("/_api/import?" + getCollectionUrlPart() +
                  "&details=true&onDuplicate=" +
                  StringUtils::urlEncode(_onDuplicateAction));
  if (isObject) {
    url += "&type=array";
  } else {
    url += "&type=documents";
  }

  if (!_fromCollectionPrefix.empty()) {
    url += "&fromPrefix=" + StringUtils::urlEncode(_fromCollectionPrefix);
  }
  if (!_toCollectionPrefix.empty()) {
    url += "&toPrefix=" + StringUtils::urlEncode(_toCollectionPrefix);
  }
  if (_firstChunk && _overwrite) {
    url += "&overwrite=true";
  }
  
  _firstChunk = false;

  std::unordered_map<std::string, std::string> headerFields;
  std::unique_ptr<SimpleHttpResult> result(_client->request(
      rest::RequestType::POST, url, str, len, headerFields));

  handleResult(result.get());
}

void ImportHelper::handleResult(SimpleHttpResult* result) {
  if (result == nullptr) {
    return;
  }

  std::shared_ptr<VPackBuilder> parsedBody;
  try {
    parsedBody = result->getBodyVelocyPack();
  } catch (...) {
    // No action required
    return;
  }
  VPackSlice const body = parsedBody->slice();

  // error details
  VPackSlice const details = body.get("details");

  if (details.isArray()) {
    for (VPackSlice const& detail : VPackArrayIterator(details)) {
      if (detail.isString()) {
        LOG(WARN) << "" << detail.copyString();
      }
    }
  }

  // get the "error" flag. This returns a pointer, not a copy
  if (arangodb::basics::VelocyPackHelper::getBooleanValue(body, "error",
                                                          false)) {
    _hasError = true;

    // get the error message
    VPackSlice const errorMessage = body.get("errorMessage");
    if (errorMessage.isString()) {
      _errorMessage = errorMessage.copyString();
    }
  }

  // look up the "created" flag
  _numberCreated += arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
      body, "created", 0);

  // look up the "errors" flag
  _numberErrors += arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
      body, "errors", 0);

  // look up the "updated" flag
  _numberUpdated += arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
      body, "updated", 0);

  // look up the "ignored" flag
  _numberIgnored += arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
      body, "ignored", 0);
}
}
}

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
#include "Basics/MutexLocker.h"
#include "Basics/OpenFilesTracker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "Import/SenderThread.h"
#include "Logger/Logger.h"
#include "Rest/GeneralResponse.h"
#include "Rest/HttpRequest.h"
#include "Shell/ClientFeature.h"
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

ImportHelper::ImportHelper(ClientFeature const* client,
                           std::string const& endpoint,
                           httpclient::SimpleHttpClientParams const& params,
                           uint64_t maxUploadSize, uint32_t threadCount)
    : _httpClient(client->createHttpClient(endpoint, params)),
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
      _rowsRead(0),
      _rowOffset(0),
      _rowsToSkip(0),
      _keyColumn(-1),
      _onDuplicateAction("error"),
      _collectionName(),
      _lineBuffer(TRI_UNKNOWN_MEM_ZONE),
      _outputBuffer(TRI_UNKNOWN_MEM_ZONE),
      _firstLine(""),
      _columnNames(),
      _hasError(false) {
  for (uint32_t i = 0; i < threadCount; i++) {
    auto http = client->createHttpClient(endpoint, params);
    _senderThreads.emplace_back(new SenderThread(std::move(http), &_stats));
    _senderThreads.back()->start();
  }
 
  // wait until all sender threads are ready 
  while (true) {
    uint32_t numReady = 0;
    for (auto const& t : _senderThreads) {
      if (t->isIdle()) {
        numReady++;
      }
    }
    if (numReady == _senderThreads.size()) {
      break;
    }
  }
}

ImportHelper::~ImportHelper() {
  for (auto const& t : _senderThreads) {
    t->beginShutdown();
  }
}

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
  _errorMessages.clear();
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
    fd = TRI_TRACKED_OPEN_FILE(fileName.c_str(), O_RDONLY | TRI_O_CLOEXEC);

    if (fd < 0) {
      _errorMessages.push_back(TRI_LAST_ERROR_STR);
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
      TRI_TRACKED_CLOSE_FILE(fd);
    }
    _errorMessages.push_back("out of memory");
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
        TRI_TRACKED_CLOSE_FILE(fd);
      }
      _errorMessages.push_back(TRI_LAST_ERROR_STR);
      return false;
    } else if (n == 0) {
      // we have read the entire file
      // now have the CSV parser parse an additional new line so it
      // will definitely process the last line of the input data if
      // it did not end with a newline
      TRI_ParseCsvString(&parser, "\n", 1);
      break;
    }

    totalRead += static_cast<int64_t>(n);
    reportProgress(totalLength, totalRead, nextProgress);

    TRI_ParseCsvString(&parser, buffer, n);
  }

  if (_outputBuffer.length() > 0) {
    sendCsvBuffer();
  }

  TRI_DestroyCsvParser(&parser);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, separator);

  if (fd != STDIN_FILENO) {
    TRI_TRACKED_CLOSE_FILE(fd);
  }

  waitForSenders();
  reportProgress(totalLength, totalRead, nextProgress);
  
  _outputBuffer.clear();
  return !_hasError;
}

bool ImportHelper::importJson(std::string const& collectionName,
                              std::string const& fileName,
                              bool assumeLinewise) {
  _collectionName = collectionName;
  _firstLine = "";
  _outputBuffer.clear();
  _errorMessages.clear();
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
    fd = TRI_TRACKED_OPEN_FILE(fileName.c_str(), O_RDONLY | TRI_O_CLOEXEC);

    if (fd < 0) {
      _errorMessages.push_back(TRI_LAST_ERROR_STR);
      return false;
    }
  }

  bool isObject = false;
  bool checkedFront = false;

  if (assumeLinewise) {
    checkedFront = true;
    isObject = false;
  }

  // progress display control variables
  int64_t totalRead = 0;
  double nextProgress = ProgressStep;

  static int const BUFFER_SIZE = 32768;

  while (!_hasError) {
    // reserve enough room to read more data
    if (_outputBuffer.reserve(BUFFER_SIZE) == TRI_ERROR_OUT_OF_MEMORY) {
      _errorMessages.push_back(TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY));

      if (fd != STDIN_FILENO) {
        TRI_TRACKED_CLOSE_FILE(fd);
      }
      return false;
    }

    // read directly into string buffer
    ssize_t n = TRI_READ(fd, _outputBuffer.end(), BUFFER_SIZE - 1);

    if (n < 0) {
      _errorMessages.push_back(TRI_LAST_ERROR_STR);
      if (fd != STDIN_FILENO) {
        TRI_TRACKED_CLOSE_FILE(fd);
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
      // objects or a JSON array with all documents)
      char const* p = _outputBuffer.begin();
      char const* e = _outputBuffer.end();

      while (p < e && (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t' ||
                       *p == '\f' || *p == '\b')) {
        ++p;
      }

      isObject = (*p == '[');
      checkedFront = true;
    }

    totalRead += static_cast<int64_t>(n);
    reportProgress(totalLength, totalRead, nextProgress);

    if (_outputBuffer.length() > _maxUploadSize) {
      if (isObject) {
        if (fd != STDIN_FILENO) {
          TRI_TRACKED_CLOSE_FILE(fd);
        }
        _errorMessages.push_back("import file is too big. please increase the value of --batch-size "
                                 "(currently " +
                                 StringUtils::itoa(_maxUploadSize) + ")");
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
    TRI_TRACKED_CLOSE_FILE(fd);
  }

  waitForSenders();
  reportProgress(totalLength, totalRead, nextProgress);
  
  MUTEX_LOCKER(guard, _stats._mutex);
  // this is an approximation only. _numberLines is more meaningful for CSV
  // imports
  _numberLines = _stats._numberErrors + _stats._numberCreated +
                 _stats._numberIgnored + _stats._numberUpdated;
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
      LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "processed " << totalRead
                                               << " bytes of input file";
      nextProcessed += 10 * 1000 * 1000;
    }
  } else {
    double pct = 100.0 * ((double)totalRead / (double)totalLength);

    if (pct >= nextProgress && totalLength >= 1024) {
      LOG_TOPIC(INFO, arangodb::Logger::FIXME)
          << "processed " << totalRead << " bytes (" << (int)nextProgress
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
    MUTEX_LOCKER(guard, _stats._mutex);
    ++_stats._numberErrors;
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
  importHelper->addField(field, fieldLength, row, column, escaped);
}

void ImportHelper::addField(char const* field, size_t fieldLength, size_t row,
                            size_t column, bool escaped) {
  if (_rowsRead < _rowsToSkip) {
    return;
  }
  // we read the first line if we get here
  if (row == _rowsToSkip) {
    std::string name = std::string(field, fieldLength);
    if (fieldLength > 0) { // translate field
      auto it = _translations.find(name);
      if (it != _translations.end()) {
        field = (*it).second.c_str();
        fieldLength = (*it).second.size();
      }
    }
    _columnNames.push_back(std::move(name));
  }
  // skip removable attributes
  if (!_removeAttributes.empty() &&
      _removeAttributes.find(_columnNames[column]) != _removeAttributes.end()) {
    return;
  }
  
  if (column > 0) {
    _lineBuffer.appendChar(',');
  }
  
  if (_keyColumn == -1 && row == _rowsToSkip && fieldLength == 4 &&
      memcmp(field, "_key", 4) == 0) {
    _keyColumn = column;
  }

  if (row == _rowsToSkip || escaped ||
      _keyColumn == static_cast<decltype(_keyColumn)>(column)) {
    // head line or escaped value
    _lineBuffer.appendJsonEncoded(field, fieldLength);
    return;
  }

  if (*field == '\0' || fieldLength == 0) {
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

  if (_convert) {
    if (IsInteger(field, fieldLength)) {
      // integer value
      // conversion might fail with out-of-range error
      try {
        if (fieldLength > 8) {
          // long integer numbers might be problematic. check if we get out of
          // range
          (void)std::stoll(std::string(field,
                                       fieldLength));  // this will fail if the
                                                       // number cannot be
                                                       // converted
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
  } else {
    if (IsInteger(field, fieldLength) || IsDecimal(field, fieldLength)) {
      // numeric value. don't convert
      _lineBuffer.appendChar('"');
      _lineBuffer.appendText(field, fieldLength);
      _lineBuffer.appendChar('"');
    } else {
      // non-numeric value
      _lineBuffer.appendJsonEncoded(field, fieldLength);
    }
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
    MUTEX_LOCKER(guard, _stats._mutex);
    ++_stats._numberErrors;
    _lineBuffer.reset();
    return;
  }

  // read a complete line

  if (_lineBuffer.length() > 0) {
    _outputBuffer.appendText(_lineBuffer);
    _lineBuffer.reset();
  } else {
    MUTEX_LOCKER(guard, _stats._mutex);
    ++_stats._numberErrors;
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
  if (!_createCollection) {
    return true;
  }

  if (!_firstChunk) {
    return true;
  }

  std::string const url("/_api/collection");

  VPackBuilder builder;
  builder.openObject();
  builder.add("name", VPackValue(_collectionName));
  builder.add("type", VPackValue(_createCollectionType == "edge" ? 3 : 2));
  builder.close();

  std::string data = builder.slice().toJson();

  std::unique_ptr<SimpleHttpResult> result(_httpClient->request(
      rest::RequestType::POST, url, data.c_str(), data.size()));

  if (result == nullptr) {
    return false;
  }

  auto code = static_cast<rest::ResponseCode>(result->getHttpReturnCode());
  if (code == rest::ResponseCode::CONFLICT || code == rest::ResponseCode::OK ||
      code == rest::ResponseCode::CREATED ||
      code == rest::ResponseCode::ACCEPTED) {
    // collection already exists or was created successfully
    return true;
  }

  std::shared_ptr<velocypack::Builder> bodyBuilder(result->getBodyVelocyPack());
  velocypack::Slice error = bodyBuilder->slice();
  if (!error.isNone()) {
    auto errorNum = error.get("errorNum").getUInt();
    auto errorMsg = error.get("errorMessage").copyString();
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "unable to create collection '" << _collectionName
        << "', server returned status code: " << static_cast<int>(code)
        << "; error [" << errorNum << "] " << errorMsg;
  } else {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "unable to create collection '" << _collectionName
        << "', server returned status code: " << static_cast<int>(code)
        << "; server returned message: " << result->getBody();
  }
  _hasError = true;
  return false;
}

bool ImportHelper::truncateCollection() {
  if (!_overwrite) {
    return true;
  }

  std::string const url = "/_api/collection/" + _collectionName + "/truncate";
  std::string data = "";// never send an completly empty string
  std::unique_ptr<SimpleHttpResult> result(_httpClient->request(
      rest::RequestType::PUT, url, data.c_str(), data.size()));

  if (result == nullptr) {
    return false;
  }

  auto code = static_cast<rest::ResponseCode>(result->getHttpReturnCode());
  if (code == rest::ResponseCode::CONFLICT || code == rest::ResponseCode::OK ||
      code == rest::ResponseCode::CREATED ||
      code == rest::ResponseCode::ACCEPTED) {
    // collection already exists or was created successfully
    return true;
  }

  LOG_TOPIC(ERR, arangodb::Logger::FIXME)
      << "unable to truncate collection '" << _collectionName
      << "', server returned status code: " << static_cast<int>(code);
  _hasError = true;
  _errorMessages.push_back("Unable to overwrite collection");
  return false;
}

void ImportHelper::sendCsvBuffer() {
  if (_hasError) {
    return;
  }

  if (!checkCreateCollection()) {
    return;
  }

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
    // url += "&overwrite=true";
    truncateCollection();
  }
  _firstChunk = false;

  SenderThread* t = findSender();
  if (t != nullptr) {
    t->sendData(url, &_outputBuffer);
  }

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
    // url += "&overwrite=true";
    truncateCollection();
  }
  _firstChunk = false;

  SenderThread* t = findSender();
  if (t != nullptr) {
    StringBuffer buff(TRI_UNKNOWN_MEM_ZONE, len, false);
    buff.appendText(str, len);
    t->sendData(url, &buff);
  }
}

SenderThread* ImportHelper::findSender() {
  while (!_senderThreads.empty()) {
    for (auto const& t : _senderThreads) {
      if (t->hasError()) {
        _hasError = true;
        _errorMessages.push_back(t->errorMessage());
        return nullptr;
      } else if (t->isIdle()) {
        return t.get();
      }
    }
    usleep(100000);
  }
  return nullptr;
}

void ImportHelper::waitForSenders() {
  while (!_senderThreads.empty()) {
    uint32_t numIdle = 0;
    for (auto const& t : _senderThreads) {
      if (t->isDone()) {
        if (t->hasError()) {
          _hasError = true;
          _errorMessages.push_back(t->errorMessage());
        }
        numIdle++;
      }
    }
    if (numIdle == _senderThreads.size()) {
      return;
    }
    usleep(10000);
  }
}
}
}

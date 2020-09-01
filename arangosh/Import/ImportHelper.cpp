////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/files.h"
#include "Basics/system-functions.h"
#include "Basics/tri-strings.h"
#include "Import/SenderThread.h"
#include "Logger/Logger.h"
#include "Rest/GeneralResponse.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Utils/ManagedDirectory.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace std::literals::string_literals;

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

ImportStatistics::ImportStatistics(application_features::ApplicationServer& server)
    : _histogram(server) {}

////////////////////////////////////////////////////////////////////////////////
/// initialize step value for progress reports
////////////////////////////////////////////////////////////////////////////////

double const ImportHelper::ProgressStep = 3.0;

////////////////////////////////////////////////////////////////////////////////
/// the server has a built-in limit for the batch size
///  and will reject bigger HTTP request bodies
////////////////////////////////////////////////////////////////////////////////

unsigned const ImportHelper::MaxBatchSize = 768 * 1024 * 1024;

////////////////////////////////////////////////////////////////////////////////
/// constructor and destructor
////////////////////////////////////////////////////////////////////////////////

ImportHelper::ImportHelper(ClientFeature const& client, std::string const& endpoint,
                           httpclient::SimpleHttpClientParams const& params,
                           uint64_t maxUploadSize, uint32_t threadCount, bool autoUploadSize)
    : _clientFeature(client),
      _httpClient(client.createHttpClient(endpoint, params)),
      _maxUploadSize(maxUploadSize),
      _periodByteCount(0),
      _autoUploadSize(autoUploadSize),
      _threadCount(threadCount),
      _tempBuffer(false),
      _separator(","),
      _quote("\""),
      _createCollectionType("document"),
      _useBackslash(false),
      _convert(true),
      _createCollection(false),
      _overwrite(false),
      _progress(false),
      _firstChunk(true),
      _ignoreMissing(false),
      _skipValidation(false),
      _numberLines(0),
      _stats(client.server()),
      _rowsRead(0),
      _rowOffset(0),
      _rowsToSkip(0),
      _keyColumn(-1),
      _onDuplicateAction("error"),
      _collectionName(),
      _lineBuffer(false),
      _outputBuffer(false),
      _firstLine(""),
      _columnNames(),
      _hasError(false) {
  for (uint32_t i = 0; i < threadCount; i++) {
    auto http = client.createHttpClient(endpoint, params);
    _senderThreads.emplace_back(
        new SenderThread(client.server(), std::move(http), &_stats, [this]() {
          CONDITION_LOCKER(guard, _threadsCondition);
          guard.signal();
        }));
    _senderThreads.back()->start();
  }

  // should self tuning code activate?
  if (_autoUploadSize) {
    _autoTuneThread.reset(new AutoTuneThread(client.server(), *this));
    _autoTuneThread->start();
  }  // if

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
                                   std::string const& pathName,
                                   DelimitedImportType typeImport) {
  ManagedDirectory directory(_clientFeature.server(), TRI_Dirname(pathName),
                             false, false, true);
  std::string fileName(TRI_Basename(pathName.c_str()));
  _collectionName = collectionName;
  _firstLine = "";
  _outputBuffer.clear();
  _lineBuffer.clear();
  _errorMessages.clear();
  _hasError = false;

  if (!checkCreateCollection()) {
    return false;
  }
  if (!collectionExists()) {
    return false;
  }

  // read and convert
  // read and convert
  int64_t totalLength;
  std::unique_ptr<arangodb::ManagedDirectory::File> fd;

  if (fileName == "-") {
    // we don't have a filesize
    totalLength = 0;
    fd = directory.readableFile(STDIN_FILENO);
  } else {
    // read filesize
    totalLength = TRI_SizeFile(pathName.c_str());
    fd = directory.readableFile(TRI_Basename(pathName.c_str()), 0);

    if (!fd) {
      _errorMessages.push_back(TRI_LAST_ERROR_STR);
      return false;
    }
  }

  // progress display control variables
  double nextProgress = ProgressStep;

  size_t separatorLength;
  char* separator = TRI_UnescapeUtf8String(_separator.c_str(), _separator.size(),
                                           &separatorLength, true);

  if (separator == nullptr) {
    _errorMessages.push_back("out of memory");
    return false;
  }

  TRI_csv_parser_t parser;

  TRI_InitCsvParser(&parser, ProcessCsvBegin, ProcessCsvAdd, ProcessCsvEnd, nullptr);

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
    ssize_t n = fd->read(buffer, sizeof(buffer));

    if (n < 0) {
      TRI_Free(separator);
      TRI_DestroyCsvParser(&parser);
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

    reportProgress(totalLength, fd->offset(), nextProgress);

    TRI_ParseCsvString(&parser, buffer, n);
  }

  if (_outputBuffer.length() > 0) {
    sendCsvBuffer();
  }

  TRI_DestroyCsvParser(&parser);
  TRI_Free(separator);

  waitForSenders();
  reportProgress(totalLength, fd->offset(), nextProgress);

  _outputBuffer.clear();
  return !_hasError;
}

bool ImportHelper::importJson(std::string const& collectionName,
                              std::string const& pathName, bool assumeLinewise) {
  ManagedDirectory directory(_clientFeature.server(), TRI_Dirname(pathName),
                             false, false, true);
  std::string fileName(TRI_Basename(pathName.c_str()));
  _collectionName = collectionName;
  _firstLine = "";
  _outputBuffer.clear();
  _errorMessages.clear();
  _hasError = false;

  if (!checkCreateCollection()) {
    return false;
  }
  if (!collectionExists()) {
    return false;
  }

  // read and convert
  int64_t totalLength;
  std::unique_ptr<arangodb::ManagedDirectory::File> fd;

  if (fileName == "-") {
    // we don't have a filesize
    totalLength = 0;
    fd = directory.readableFile(STDIN_FILENO);
  } else {
    // read filesize
    totalLength = TRI_SizeFile(pathName.c_str());
    fd = directory.readableFile(TRI_Basename(fileName.c_str()), 0);

    if (!fd) {
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
  double nextProgress = ProgressStep;

  static int const BUFFER_SIZE = 32768;
  _rowOffset = 0;
  _rowsRead = 0;

  while (!_hasError) {
    // reserve enough room to read more data
    if (_outputBuffer.reserve(BUFFER_SIZE) == TRI_ERROR_OUT_OF_MEMORY) {
      _errorMessages.push_back(TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY));

      return false;
    }

    // read directly into string buffer
    ssize_t n = fd->read(_outputBuffer.end(), BUFFER_SIZE - 1);

    if (n < 0) {
      _errorMessages.push_back(TRI_LAST_ERROR_STR);
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

    reportProgress(totalLength, fd->offset(), nextProgress);

    if (_outputBuffer.length() > _maxUploadSize) {
      if (isObject) {
        _errorMessages.push_back(
            "import file is too big. please increase the value of --batch-size "
            "(currently " +
            StringUtils::itoa(_maxUploadSize) + ")");
        return false;
      }

      // send all data before last '\n'
      char const* first = _outputBuffer.c_str();
      char const * pos = static_cast<char const*>(memrchr(first, '\n', _outputBuffer.length()));

      if (pos != nullptr) {
        size_t len = pos - first + 1;
        char const * cursor = first;
        do {
          ++cursor;
          cursor = static_cast<char const*>(memchr(cursor, '\n', pos - cursor));
          ++_rowsRead;
        } while (nullptr != cursor);
        sendJsonBuffer(first, len, isObject);
        _outputBuffer.erase_front(len);
        _rowOffset = _rowsRead;
      }
    }
  }

  if (_outputBuffer.length() > 0) {
    ++_rowsRead;
    sendJsonBuffer(_outputBuffer.c_str(), _outputBuffer.length(), isObject);
  }

  waitForSenders();
  reportProgress(totalLength, fd->offset(), nextProgress);

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

void ImportHelper::reportProgress(int64_t totalLength, int64_t totalRead, double& nextProgress) {
  if (!_progress) {
    return;
  }

  if (totalLength == 0) {
    // length of input is unknown
    // in this case we cannot report the progress as a percentage
    // instead, report every 10 MB processed
    static int64_t nextProcessed = 10 * 1000 * 1000;

    if (totalRead >= nextProcessed) {
      LOG_TOPIC("c0e6e", INFO, arangodb::Logger::FIXME)
          << "processed " << totalRead << " bytes of input file";
      nextProcessed += 10 * 1000 * 1000;
    }
  } else {
    double pct = 100.0 * ((double)totalRead / (double)totalLength);

    if (pct >= nextProgress && totalLength >= 1024) {
      LOG_TOPIC("9ddf3", INFO, arangodb::Logger::FIXME)
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

void ImportHelper::ProcessCsvAdd(TRI_csv_parser_t* parser, char const* field, size_t fieldLength,
                                 size_t row, size_t column, bool escaped) {
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
    if (fieldLength > 0) {  // translate field
      auto it = _translations.find(name);
      if (it != _translations.end()) {
        field = (*it).second.c_str();
        fieldLength = (*it).second.size();
      }
    }
    _columnNames.push_back(std::move(name));
  }
  // skip removable attributes
  if (!_removeAttributes.empty() && column < _columnNames.size() &&
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
  if (fieldLength == 4 && (memcmp(field, "true", 4) == 0 || memcmp(field, "null", 4) == 0)) {
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

void ImportHelper::ProcessCsvEnd(TRI_csv_parser_t* parser, char const* field, size_t fieldLength,
                                 size_t row, size_t column, bool escaped) {
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
    _firstLine = std::string(_lineBuffer.c_str(), _lineBuffer.length());
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

bool ImportHelper::collectionExists() {
  std::string const url("/_api/collection/" + StringUtils::urlEncode(_collectionName));
  std::unique_ptr<SimpleHttpResult> result(
      _httpClient->request(rest::RequestType::GET, url, nullptr, 0));

  if (result == nullptr) {
    return false;
  }

  auto code = static_cast<rest::ResponseCode>(result->getHttpReturnCode());
  if (code == rest::ResponseCode::OK || code == rest::ResponseCode::CREATED ||
      code == rest::ResponseCode::ACCEPTED) {
    // collection already exists or was created successfully
    return true;
  }

  std::shared_ptr<velocypack::Builder> bodyBuilder(result->getBodyVelocyPack());
  velocypack::Slice error = bodyBuilder->slice();
  if (!error.isNone()) {
    auto errorNum = error.get(StaticStrings::ErrorNum).getUInt();
    auto errorMsg = error.get(StaticStrings::ErrorMessage).copyString();
    LOG_TOPIC("f2c4a", ERR, arangodb::Logger::FIXME)
        << "unable to access collection '" << _collectionName
        << "', server returned status code: " << static_cast<int>(code)
        << "; error [" << errorNum << "] " << errorMsg;
  } else {
    LOG_TOPIC("57d57", ERR, arangodb::Logger::FIXME)
        << "unable to accesss collection '" << _collectionName
        << "', server returned status code: " << static_cast<int>(code)
        << "; server returned message: "
        << Logger::CHARS(result->getBody().c_str(), result->getBody().length());
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if we must create the target collection, and create it if
/// required
////////////////////////////////////////////////////////////////////////////////

bool ImportHelper::checkCreateCollection() {
  if (!_createCollection) {
    return true;
  }

  std::string const url("/_api/collection");
  VPackBuilder builder;

  builder.openObject();
  builder.add(arangodb::StaticStrings::DataSourceName,
              arangodb::velocypack::Value(_collectionName));
  builder.add(arangodb::StaticStrings::DataSourceType,
              arangodb::velocypack::Value(_createCollectionType == "edge" ? 3 : 2));
  builder.close();

  std::string data = builder.slice().toJson();
  std::unique_ptr<SimpleHttpResult> result(
      _httpClient->request(rest::RequestType::POST, url, data.c_str(), data.size()));

  if (result == nullptr) {
    return false;
  }

  auto code = static_cast<rest::ResponseCode>(result->getHttpReturnCode());
  if (code == rest::ResponseCode::CONFLICT || code == rest::ResponseCode::OK ||
      code == rest::ResponseCode::CREATED || code == rest::ResponseCode::ACCEPTED) {
    // collection already exists or was created successfully
    return true;
  }

  std::shared_ptr<velocypack::Builder> bodyBuilder(result->getBodyVelocyPack());
  velocypack::Slice error = bodyBuilder->slice();
  if (!error.isNone()) {
    auto errorNum = error.get(StaticStrings::ErrorNum).getUInt();
    auto errorMsg = error.get(StaticStrings::ErrorMessage).copyString();
    LOG_TOPIC("09478", ERR, arangodb::Logger::FIXME)
        << "unable to create collection '" << _collectionName
        << "', server returned status code: " << static_cast<int>(code)
        << "; error [" << errorNum << "] " << errorMsg;
  } else {
    LOG_TOPIC("2211f", ERR, arangodb::Logger::FIXME)
        << "unable to create collection '" << _collectionName
        << "', server returned status code: " << static_cast<int>(code)
        << "; server returned message: "
        << Logger::CHARS(result->getBody().c_str(), result->getBody().length());
  }
  _hasError = true;
  return false;
}

bool ImportHelper::truncateCollection() {
  if (!_overwrite) {
    return true;
  }

  std::string const url = "/_api/collection/" + _collectionName + "/truncate";
  std::string data = "";  // never send an completly empty string
  std::unique_ptr<SimpleHttpResult> result(
      _httpClient->request(rest::RequestType::PUT, url, data.c_str(), data.size()));

  if (result == nullptr) {
    return false;
  }

  auto code = static_cast<rest::ResponseCode>(result->getHttpReturnCode());
  if (code == rest::ResponseCode::CONFLICT || code == rest::ResponseCode::OK ||
      code == rest::ResponseCode::CREATED || code == rest::ResponseCode::ACCEPTED) {
    // collection already exists or was created successfully
    return true;
  }

  LOG_TOPIC("f8ae4", ERR, arangodb::Logger::FIXME)
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

  std::string url("/_api/import?" + getCollectionUrlPart() +
                  "&line=" + StringUtils::itoa(_rowOffset) +
                  "&details=true&onDuplicate=" + StringUtils::urlEncode(_onDuplicateAction) +
                  "&ignoreMissing=" + (_ignoreMissing ? "true" : "false"));

  if (!_fromCollectionPrefix.empty()) {
    url += "&fromPrefix=" + StringUtils::urlEncode(_fromCollectionPrefix);
  }
  if (!_toCollectionPrefix.empty()) {
    url += "&toPrefix=" + StringUtils::urlEncode(_toCollectionPrefix);
  }
  if (_skipValidation) {
    url += "&"s + StaticStrings::SkipDocumentValidation + "=true";
  }
  if (_firstChunk && _overwrite) {
    // url += "&overwrite=true";
    truncateCollection();
  }
  _firstChunk = false;

  SenderThread* t = findIdleSender();
  if (t != nullptr) {
    uint64_t tmp_length = _outputBuffer.length();
    t->sendData(url, &_outputBuffer, _rowOffset + 1, _rowsRead);
    addPeriodByteCount(tmp_length + url.length());
  }

  _outputBuffer.reset();
  _rowOffset = _rowsRead;
}

void ImportHelper::sendJsonBuffer(char const* str, size_t len, bool isObject) {
  if (_hasError) {
    return;
  }

  // build target url
  std::string url("/_api/import?" + getCollectionUrlPart() +
                  "&details=true&onDuplicate=" + StringUtils::urlEncode(_onDuplicateAction));
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
  if (_skipValidation) {
    url += "&"s + StaticStrings::SkipDocumentValidation + "=true";
  }

  _firstChunk = false;

  SenderThread* t = findIdleSender();
  if (t != nullptr) {
    _tempBuffer.reset();
    _tempBuffer.appendText(str, len);
    t->sendData(url, &_tempBuffer, _rowOffset +1, _rowsRead);
    addPeriodByteCount(len + url.length());
  }
}

/// Should return an idle sender, collect all errors
/// and return nullptr, if there was an error
SenderThread* ImportHelper::findIdleSender() {
  if (_autoUploadSize) {
    _autoTuneThread->paceSends();
  }  // if

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

    CONDITION_LOCKER(guard, _threadsCondition);
    guard.wait(10000);
  }
  return nullptr;
}

/// Busy wait for all sender threads to finish
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
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}
}  // namespace import
}  // namespace arangodb

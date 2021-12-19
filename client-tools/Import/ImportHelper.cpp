////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/application-exit.h"
#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "Basics/system-functions.h"
#include "Basics/tri-strings.h"
#include "Import/SenderThread.h"
#include "Logger/Logger.h"
#include "Rest/GeneralResponse.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/HttpResponseChecker.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Utils/ManagedDirectory.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace std::literals::string_literals;

/// @brief helper function to determine if a field value is an integer
/// this function is here to avoid usage of regexes, which are too slow
namespace {
bool isInteger(char const* field, size_t fieldLength) {
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

/// @brief helper function to determine if a field value maybe is a decimal
/// value. this function peeks into the first few bytes of the value only
/// this function is here to avoid usage of regexes, which are too slow
bool isDecimal(char const* field, size_t fieldLength) {
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

}  // namespace

namespace arangodb {
namespace import {

ImportStatistics::ImportStatistics(
    application_features::ApplicationServer& server)
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

ImportHelper::ImportHelper(ClientFeature const& client,
                           std::string const& endpoint,
                           httpclient::SimpleHttpClientParams const& params,
                           uint64_t maxUploadSize, uint32_t threadCount,
                           bool autoUploadSize)
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
      _hasError(false),
      _headersSeen(false),
      _emittedField(false) {
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
    _autoTuneThread = std::make_unique<AutoTuneThread>(client.server(), *this);
    _autoTuneThread->start();
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

// read headers from separate file
bool ImportHelper::readHeadersFile(std::string const& headersFile,
                                   DelimitedImportType typeImport,
                                   char separator) {
  TRI_ASSERT(!headersFile.empty());
  TRI_ASSERT(!_headersSeen);

  ManagedDirectory directory(_clientFeature.server(), TRI_Dirname(headersFile),
                             false, false, false);
  if (directory.status().fail()) {
    _errorMessages.emplace_back(directory.status().errorMessage());
    return false;
  }

  std::string fileName(TRI_Basename(headersFile.c_str()));
  std::unique_ptr<arangodb::ManagedDirectory::File> fd =
      directory.readableFile(fileName.c_str(), 0);
  if (!fd) {
    _errorMessages.push_back(TRI_LAST_ERROR_STR);
    return false;
  }

  // make a copy of _rowsToSkip
  size_t rowsToSkip = _rowsToSkip;
  _rowsToSkip = 0;

  TRI_csv_parser_t parser;
  TRI_InitCsvParser(&parser, ProcessCsvBegin, ProcessCsvAdd, ProcessCsvEnd,
                    nullptr);
  TRI_SetSeparatorCsvParser(&parser, separator);
  TRI_UseBackslashCsvParser(&parser, _useBackslash);

  // in csv, we'll use the quote char if set
  // in tsv, we do not use the quote char
  if (typeImport == ImportHelper::CSV && _quote.size() > 0) {
    TRI_SetQuoteCsvParser(&parser, _quote[0], true);
  } else {
    TRI_SetQuoteCsvParser(&parser, '\0', false);
  }
  parser._dataAdd = this;

  auto guard =
      scopeGuard([&parser]() noexcept { TRI_DestroyCsvParser(&parser); });

  constexpr int BUFFER_SIZE = 16384;
  char buffer[BUFFER_SIZE];

  while (!_hasError) {
    ssize_t n = fd->read(buffer, sizeof(buffer));

    if (n < 0) {
      _errorMessages.push_back(TRI_LAST_ERROR_STR);
      return false;
    }
    if (n == 0) {
      // we have read the entire file
      // now have the CSV parser parse an additional new line so it
      // will definitely process the last line of the input data if
      // it did not end with a newline
      TRI_ParseCsvString(&parser, "\n", 1);
      break;
    }

    TRI_ParseCsvString(&parser, buffer, n);
  }

  if (_outputBuffer.size() > 0 &&
      *(_outputBuffer.begin() + _outputBuffer.size() - 1) != '\n') {
    // add a newline to finish the headers line
    _outputBuffer.appendChar('\n');
  }

  if (_rowsRead > 2) {
    _errorMessages.push_back("headers file '" + headersFile +
                             "' contained more than a single line of headers");
    return false;
  }

  // reset our state properly
  _lineBuffer.clear();
  _headersSeen = true;
  _emittedField = false;
  _rowOffset = 0;
  _rowsRead = 0;
  _numberLines = 0;
  // restore copy of _rowsToSkip
  _rowsToSkip = rowsToSkip;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief imports a delimited file
////////////////////////////////////////////////////////////////////////////////

bool ImportHelper::importDelimited(std::string const& collectionName,
                                   std::string const& pathName,
                                   std::string const& headersFile,
                                   DelimitedImportType typeImport) {
  ManagedDirectory directory(_clientFeature.server(), TRI_Dirname(pathName),
                             false, false, true);
  if (directory.status().fail()) {
    _errorMessages.emplace_back(directory.status().errorMessage());
    return false;
  }

  std::string fileName(TRI_Basename(pathName.c_str()));
  _collectionName = collectionName;
  _firstLine = "";
  _outputBuffer.clear();
  _lineBuffer.clear();
  _errorMessages.clear();
  _hasError = false;
  _headersSeen = false;
  _emittedField = false;
  _rowOffset = 0;
  _rowsRead = 0;
  _numberLines = 0;

  if (!checkCreateCollection()) {
    return false;
  }
  if (!collectionExists()) {
    return false;
  }

  // handle separator
  char separator;
  {
    size_t separatorLength;
    char* s = TRI_UnescapeUtf8String(_separator.c_str(), _separator.size(),
                                     &separatorLength, true);

    if (s == nullptr) {
      _errorMessages.push_back("out of memory");
      return false;
    }

    separator = s[0];
    TRI_Free(s);
  }

  if (!headersFile.empty() &&
      !readHeadersFile(headersFile, typeImport, separator)) {
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
    fd = directory.readableFile(fileName.c_str(), 0);

    if (!fd) {
      _errorMessages.push_back(TRI_LAST_ERROR_STR);
      return false;
    }
  }

  // progress display control variables
  double nextProgress = ProgressStep;

  TRI_csv_parser_t parser;
  TRI_InitCsvParser(&parser, ProcessCsvBegin, ProcessCsvAdd, ProcessCsvEnd,
                    nullptr);
  TRI_SetSeparatorCsvParser(&parser, separator);
  TRI_UseBackslashCsvParser(&parser, _useBackslash);

  // in csv, we'll use the quote char if set
  // in tsv, we do not use the quote char
  if (typeImport == ImportHelper::CSV && _quote.size() > 0) {
    TRI_SetQuoteCsvParser(&parser, _quote[0], true);
  } else {
    TRI_SetQuoteCsvParser(&parser, '\0', false);
  }
  parser._dataAdd = this;

  constexpr int BUFFER_SIZE = 262144;
  char buffer[BUFFER_SIZE];

  while (!_hasError) {
    ssize_t n = fd->read(buffer, sizeof(buffer));

    if (n < 0) {
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

  waitForSenders();
  reportProgress(totalLength, fd->offset(), nextProgress);

  _outputBuffer.clear();
  return !_hasError;
}

bool ImportHelper::importJson(std::string const& collectionName,
                              std::string const& pathName,
                              bool assumeLinewise) {
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
    fd = directory.readableFile(fileName.c_str(), 0);

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

  constexpr int BUFFER_SIZE = 1048576;

  while (!_hasError) {
    // reserve enough room to read more data
    if (_outputBuffer.reserve(BUFFER_SIZE) == TRI_ERROR_OUT_OF_MEMORY) {
      _errorMessages.emplace_back(TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY));

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

    uint64_t maxUploadSize = getMaxUploadSize();

    if (_outputBuffer.length() > maxUploadSize) {
      if (isObject) {
        _errorMessages.push_back(
            "import file is too big. please increase the value of --batch-size "
            "(currently " +
            StringUtils::itoa(maxUploadSize) + ")");
        return false;
      }

      // send all data before last '\n'
      char const* first = _outputBuffer.c_str();
      char const* pos = static_cast<char const*>(
          memrchr(first, '\n', _outputBuffer.length()));

      if (pos != nullptr) {
        size_t len = pos - first + 1;
        char const* cursor = first;
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

void ImportHelper::reportProgress(int64_t totalLength, int64_t totalRead,
                                  double& nextProgress) {
  if (!_progress) {
    return;
  }

  using arangodb::basics::StringUtils::formatSize;

  if (totalLength == 0) {
    // length of input is unknown
    // in this case we cannot report the progress as a percentage
    // instead, report every 10 MB processed
    static int64_t nextProcessed = 10 * 1000 * 1000;

    if (totalRead >= nextProcessed) {
      LOG_TOPIC("c0e6e", INFO, arangodb::Logger::FIXME)
          << "processed " << formatSize(totalRead) << " of input file";
      nextProcessed += 10 * 1000 * 1000;
    }
  } else {
    double pct = 100.0 * ((double)totalRead / (double)totalLength);

    if (pct >= nextProgress && totalLength >= 1024) {
      LOG_TOPIC("9ddf3", INFO, arangodb::Logger::FIXME)
          << "processed " << formatSize(totalRead) << " (" << (int)nextProgress
          << "%) of input file";
      nextProgress = (double)((int)(pct + ProgressStep));
    }
  }
}

void ImportHelper::verifyNestedAttributes(std::string const& input,
                                          std::string const& key) const {
  if (input == key) {
    LOG_TOPIC("4f701", FATAL, arangodb::Logger::FIXME)
        << "Wrong syntax in --merge-attributes: cannot nest attributes";
    FATAL_ERROR_EXIT();
  }
}

void ImportHelper::verifyMergeAttributesSyntax(std::string const& input) const {
  if (input.find_first_of("[]=") != std::string::npos) {
    LOG_TOPIC("0b9e2", FATAL, arangodb::Logger::FIXME)
        << "Wrong syntax in --merge-attributes: attribute names and literals "
           "cannot contain any of '[', ']' or '='";
    FATAL_ERROR_EXIT();
  }
}

std::vector<ImportHelper::Step> ImportHelper::tokenizeInput(
    std::string const& input, std::string const& key) const {
  std::vector<Step> steps;
  std::size_t pos = 0;

  while (pos < input.size()) {
    std::size_t pos1 = input.find('[', pos);
    std::size_t pos2 = input.find(']', pos);
    bool found1 = pos1 != std::string::npos;
    bool found2 = pos2 != std::string::npos;
    if (found1 != found2) {
      LOG_DEVEL << "IF";
      LOG_TOPIC("89a3b", FATAL, arangodb::Logger::FIXME)
          << "Wrong syntax in --merge-attributes: unbalanced brackets";
      FATAL_ERROR_EXIT();
    }
    if (found1) {
      // reference, [...]
      assert(found2);
      if (pos1 > pos2) {
        LOG_TOPIC("db7aa", FATAL, arangodb::Logger::FIXME)
            << "Wrong syntax in --merge-attributes";
        FATAL_ERROR_EXIT();
      }
      if (pos1 + 1 == pos2) {
        LOG_TOPIC("f1a42", FATAL, arangodb::Logger::FIXME)
            << "Wrong syntax in --merge-attributes: empty argument '[]' not "
               "allowed";
        FATAL_ERROR_EXIT();
      }
      if (pos1 != pos) {
        std::string inputSubstr = input.substr(pos, pos1 - pos);
        verifyMergeAttributesSyntax(inputSubstr);
        steps.emplace_back(std::move(inputSubstr), true);
      }
      std::string inputSubstr = input.substr(pos1 + 1, pos2 - pos1 - 1);
      verifyMergeAttributesSyntax(inputSubstr);
      verifyNestedAttributes(std::move(inputSubstr), key);
      steps.emplace_back(std::move(inputSubstr), false);
      pos = pos2 + 1;
    } else {
      // literal
      std::string inputSubstr = input.substr(pos, input.size() - pos);
      verifyMergeAttributesSyntax(inputSubstr);
      steps.emplace_back(std::move(inputSubstr), true);
      pos = input.size();
    }
  }
  return steps;
}

void ImportHelper::parseMergeAttributes(std::vector<std::string> const& args) {
  for (auto const& arg : args) {
    std::vector<std::string> tokenizedArgByAssignment =
        StringUtils::split(arg, "=");
    if (tokenizedArgByAssignment.size() != 2) {
      LOG_TOPIC("ae6dc", FATAL, arangodb::Logger::FIXME)
          << "Wrong syntax in --merge-attributes: Unexpected number of '=' "
             "characters found";
      FATAL_ERROR_EXIT();
    }
    std::vector<ImportHelper::Step> tokenizedArg =
        tokenizeInput(tokenizedArgByAssignment[1], tokenizedArgByAssignment[0]);
    _mergeAttributesInstructions.emplace_back(tokenizedArgByAssignment[0],
                                              std::move(tokenizedArg));
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
  _fieldsLookUpTable.clear();
  if (_lineBuffer.length() > 0) {
    // error
    MUTEX_LOCKER(guard, _stats._mutex);
    ++_stats._numberErrors;
    _lineBuffer.clear();
  }

  ++_numberLines;
  _emittedField = false;

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
    // still some rows left to skip over
    return;
  }

  // we are reading the first line if we get here
  if (row == _rowsToSkip && !_headersSeen) {
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

  std::string lookUpTableValue;

  auto guard = scopeGuard([&]() noexcept {
    if (!_mergeAttributesInstructions.empty()) {
      _fieldsLookUpTable.try_emplace(_columnNames[column],
                                     std::move(lookUpTableValue));
    }
  });

  // we will write out this attribute!

  if (column > 0 && _emittedField) {
    _lineBuffer.appendChar(',');
  }

  _emittedField = true;

  if (_keyColumn == -1 && row == _rowsToSkip && !_headersSeen &&
      fieldLength == 4 && memcmp(field, "_key", 4) == 0) {
    _keyColumn = column;
  }

  // check if a datatype was forced for this attribute
  auto itTypes = _datatypes.end();
  if (!_datatypes.empty() && column < _columnNames.size()) {
    itTypes = _datatypes.find(_columnNames[column]);
  }

  if (!_mergeAttributesInstructions.empty()) {
    lookUpTableValue = field;
  }

  if ((row == _rowsToSkip && !_headersSeen) ||
      (escaped && itTypes == _datatypes.end()) ||
      _keyColumn == static_cast<decltype(_keyColumn)>(column)) {
    // headline or escaped value
    _lineBuffer.appendJsonEncoded(field, fieldLength);
    return;
  }

  // check if a datatype was forced for this attribute
  if (itTypes != _datatypes.end()) {
    std::string const& datatype = (*itTypes).second;
    if (datatype == "number") {
      if (::isInteger(field, fieldLength) || ::isDecimal(field, fieldLength)) {
        _lineBuffer.appendText(field, fieldLength);
      } else {
        if (!_mergeAttributesInstructions.empty()) {
          lookUpTableValue = "0";
        }
        _lineBuffer.appendText("0", 1);
      }
    } else if (datatype == "boolean") {
      if ((fieldLength == 5 && memcmp(field, "false", 5) == 0) ||
          (fieldLength == 4 && memcmp(field, "null", 4) == 0) ||
          (fieldLength == 1 && *field == '0')) {
        if (!_mergeAttributesInstructions.empty()) {
          lookUpTableValue = "false";
        }
        _lineBuffer.appendText("false", 5);
      } else {
        if (!_mergeAttributesInstructions.empty()) {
          lookUpTableValue = "true";
        }
        _lineBuffer.appendText("true", 4);
      }
    } else if (datatype == "null") {
      if (!_mergeAttributesInstructions.empty()) {
        lookUpTableValue = "null";
      }
      _lineBuffer.appendText("null", 4);
    } else {
      // string
      TRI_ASSERT(datatype == "string");
      _lineBuffer.appendJsonEncoded(field, fieldLength);
    }
    return;
  }

  if (*field == '\0' || fieldLength == 0) {
    // do nothing
    _lineBuffer.appendText("null", 4);
    if (!_mergeAttributesInstructions.empty()) {
      lookUpTableValue = "null";
    }
    return;
  }

  // automatic detection of datatype based on value (--convert)
  if (_convert) {
    // check for literals null, false and true
    if ((fieldLength == 4 &&
         (memcmp(field, "true", 4) == 0 || memcmp(field, "null", 4) == 0)) ||
        (fieldLength == 5 && memcmp(field, "false", 5) == 0)) {
      _lineBuffer.appendText(field, fieldLength);
    } else if (::isInteger(field, fieldLength)) {
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
        if (!_mergeAttributesInstructions.empty()) {
          lookUpTableValue = std::to_string(num);
        }
        _lineBuffer.appendInteger(num);
      } catch (...) {
        // conversion failed
        _lineBuffer.appendJsonEncoded(field, fieldLength);
      }
    } else if (::isDecimal(field, fieldLength)) {
      // double value
      // conversion might fail with out-of-range error
      try {
        std::string tmp(field, fieldLength);
        size_t pos = 0;
        double num = std::stod(tmp, &pos);
        if (pos == fieldLength) {
          bool failed = (num != num || num == HUGE_VAL || num == -HUGE_VAL);
          if (!failed) {
            if (!_mergeAttributesInstructions.empty()) {
              lookUpTableValue = std::to_string(num);
            }
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
    if (::isInteger(field, fieldLength) || ::isDecimal(field, fieldLength)) {
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

  addField(field, fieldLength, row, column++, escaped);

  // add --merge-attributes arguments
  if (!_mergeAttributesInstructions.empty()) {
    for (auto& [key, value] : _mergeAttributesInstructions) {
      if (row == _rowsToSkip) {
        std::for_each(
            value.begin(), value.end(),
            [this, key = &key](Step const& attrProperties) {
              if (!attrProperties.isLiteral) {
                if (std::find(_columnNames.begin(), _columnNames.end(),
                              attrProperties.value) == _columnNames.end()) {
                  LOG_TOPIC("ab353", WARN, arangodb::Logger::FIXME)
                      << "In --merge-attributes: No matching value for "
                         "attribute name "
                      << attrProperties.value << " to populate attribute "
                      << key;
                }
              }
            });
        addField(key.c_str(), key.size(), row, column, escaped);
      } else {
        std::string attrsToMerge;
        std::for_each(
            value.begin(), value.end(),
            [this, &attrsToMerge](Step const& attrProperties) {
              if (!attrProperties.isLiteral) {
                if (auto it = _fieldsLookUpTable.find(attrProperties.value);
                    it != _fieldsLookUpTable.end()) {
                  attrsToMerge += it->second;
                }
              } else {
                attrsToMerge += attrProperties.value;
              }
            });
        bool tmp = _convert;
        _convert =
            false;  // force only --merge-attribute arguments to be treated as
                    // string then switch back to normal conversion
        addField(attrsToMerge.c_str(), attrsToMerge.size(), row, column,
                 escaped);
        _convert = tmp;
      }
      column++;
    }
  }

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

  if (_outputBuffer.length() > getMaxUploadSize()) {
    sendCsvBuffer();
    _outputBuffer.appendText(_firstLine);
  }
}

bool ImportHelper::collectionExists() {
  std::string const url("/_api/collection/" +
                        StringUtils::urlEncode(_collectionName));
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

  auto check = arangodb::HttpResponseChecker::check(
      _httpClient->getErrorMessage(), result.get());

  if (check.fail()) {
    LOG_TOPIC("f2c4a", ERR, arangodb::Logger::FIXME)
        << "unable to access collection '" << _collectionName << "', "
        << check.errorMessage();
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
  builder.add(
      arangodb::StaticStrings::DataSourceType,
      arangodb::velocypack::Value(_createCollectionType == "edge" ? 3 : 2));
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

  auto check = arangodb::HttpResponseChecker::check(
      _httpClient->getErrorMessage(), result.get());
  if (check.fail()) {
    LOG_TOPIC("09478", ERR, arangodb::Logger::FIXME)
        << "unable to create collection '" << _collectionName << "', "
        << check.errorMessage();
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

  auto check = arangodb::HttpResponseChecker::check(
      _httpClient->getErrorMessage(), result.get());
  if (check.fail()) {
    LOG_TOPIC("f8ae4", ERR, arangodb::Logger::FIXME)
        << "unable to truncate collection '" << _collectionName << "', "
        << check.errorMessage();
  }
  _hasError = true;
  _errorMessages.push_back("Unable to overwrite collection");
  return false;
}

void ImportHelper::sendCsvBuffer() {
  if (_hasError) {
    return;
  }

  std::string url("/_api/import?" + getCollectionUrlPart() + "&line=" +
                  StringUtils::itoa(_rowOffset) + "&details=true&onDuplicate=" +
                  StringUtils::urlEncode(_onDuplicateAction) +
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
  if (_skipValidation) {
    url += "&"s + StaticStrings::SkipDocumentValidation + "=true";
  }

  _firstChunk = false;

  SenderThread* t = findIdleSender();
  if (t != nullptr) {
    _tempBuffer.reset();
    _tempBuffer.appendText(str, len);
    t->sendData(url, &_tempBuffer, _rowOffset + 1, _rowsRead);
    addPeriodByteCount(len + url.length());
  }
}

/// Should return an idle sender, collect all errors
/// and return nullptr, if there was an error
SenderThread* ImportHelper::findIdleSender() {
  if (_autoUploadSize) {
    _autoTuneThread->paceSends();
  }

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

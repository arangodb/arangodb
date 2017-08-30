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

#ifndef ARANGODB_IMPORT_IMPORT_HELPER_H
#define ARANGODB_IMPORT_IMPORT_HELPER_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"

#include "Basics/StringBuffer.h"
#include "Basics/csv.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

namespace arangodb {
class ClientFeature;
namespace httpclient {
class SimpleHttpClient;
class SimpleHttpResult;
struct SimpleHttpClientParams;
}
}

////////////////////////////////////////////////////////////////////////////////
/// @brief class for http requests
////////////////////////////////////////////////////////////////////////////////

namespace arangodb {
namespace import {
class SenderThread;

struct ImportStatistics {
  size_t _numberCreated = 0;
  size_t _numberErrors = 0;
  size_t _numberUpdated = 0;
  size_t _numberIgnored = 0;

  arangodb::Mutex _mutex;
};

class ImportHelper {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief type of delimited import
  //////////////////////////////////////////////////////////////////////////////

  enum DelimitedImportType { CSV = 0, TSV };

 private:
  ImportHelper(ImportHelper const&) = delete;
  ImportHelper& operator=(ImportHelper const&) = delete;

 public:
  ImportHelper(ClientFeature const* client, std::string const& endpoint,
               httpclient::SimpleHttpClientParams const& params,
               uint64_t maxUploadSize, uint32_t threadCount);

  ~ImportHelper();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief imports a delimited file
  //////////////////////////////////////////////////////////////////////////////

  bool importDelimited(std::string const& collectionName,
                       std::string const& fileName,
                       DelimitedImportType typeImport);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief imports a file with JSON objects
  /// each line must contain a complete JSON object
  //////////////////////////////////////////////////////////////////////////////

  bool importJson(std::string const& collectionName,
                  std::string const& fileName, bool assumeLinewise);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the action to carry out on duplicate _key
  //////////////////////////////////////////////////////////////////////////////

  void setOnDuplicateAction(std::string const& action) {
    _onDuplicateAction = action;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the quote character
  ///
  /// this is a string because the quote might also be empty if not used
  //////////////////////////////////////////////////////////////////////////////

  void setQuote(std::string const& quote) { _quote = quote; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set collection name prefix for _from
  //////////////////////////////////////////////////////////////////////////////

  void setFrom(std::string const& from) { _fromCollectionPrefix = from; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set collection name prefix for _to
  //////////////////////////////////////////////////////////////////////////////

  void setTo(std::string const& to) { _toCollectionPrefix = to; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not backslashes can be used for escaping quotes
  //////////////////////////////////////////////////////////////////////////////

  void useBackslash(bool value) { _useBackslash = value; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the separator
  //////////////////////////////////////////////////////////////////////////////

  void setSeparator(std::string const& separator) { _separator = separator; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the createCollection flag
  ///
  /// @param bool value                create the collection if it does not
  /// exist
  //////////////////////////////////////////////////////////////////////////////

  void setCreateCollection(bool value) { _createCollection = value; }

  void setCreateCollectionType(std::string const& value) {
    _createCollectionType = value;
  }

  void setTranslations(
      std::unordered_map<std::string, std::string> const& translations) {
    _translations = translations;
  }
  
  void setRemoveAttributes(std::vector<std::string> const& attr) {
    for (std::string const& str : attr) {
      _removeAttributes.insert(str);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not to overwrite existing data in the collection
  //////////////////////////////////////////////////////////////////////////////

  void setOverwrite(bool value) { _overwrite = value; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set the number of rows to skip
  //////////////////////////////////////////////////////////////////////////////

  void setRowsToSkip(size_t value) { _rowsToSkip = value; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the number of rows to skip
  //////////////////////////////////////////////////////////////////////////////

  size_t getRowsToSkip() const { return _rowsToSkip; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not to convert strings that contain "null", "false",
  /// "true" or that look like numbers into those types
  //////////////////////////////////////////////////////////////////////////////

  void setConversion(bool value) { _convert = value; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set the progress indicator
  //////////////////////////////////////////////////////////////////////////////

  void setProgress(bool value) { _progress = value; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the number of lines read (meaningful for CSV only)
  //////////////////////////////////////////////////////////////////////////////

  size_t getReadLines() { return _numberLines; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the number of documents imported
  //////////////////////////////////////////////////////////////////////////////

  size_t getNumberCreated() { return _stats._numberCreated; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the number of errors
  //////////////////////////////////////////////////////////////////////////////

  size_t getNumberErrors() { return _stats._numberErrors; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the number of updated documents
  //////////////////////////////////////////////////////////////////////////////

  size_t getNumberUpdated() { return _stats._numberUpdated; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the number of ignored documents
  //////////////////////////////////////////////////////////////////////////////

  size_t getNumberIgnored() const { return _stats._numberIgnored; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief increase the row counter
  //////////////////////////////////////////////////////////////////////////////

  void incRowsRead() { ++_rowsRead; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the number of rows read
  //////////////////////////////////////////////////////////////////////////////

  size_t getRowsRead() const { return _rowsRead; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the error message
  ///
  /// @return string       get the error message
  //////////////////////////////////////////////////////////////////////////////

  std::vector<std::string> getErrorMessages() { return _errorMessages; }

 private:
  static void ProcessCsvBegin(TRI_csv_parser_t*, size_t);
  static void ProcessCsvAdd(TRI_csv_parser_t*, char const*, size_t, size_t,
                            size_t, bool);
  static void ProcessCsvEnd(TRI_csv_parser_t*, char const*, size_t, size_t,
                            size_t, bool);

  void reportProgress(int64_t, int64_t, double&);

  std::string getCollectionUrlPart() const;
  void beginLine(size_t row);
  void addField(char const*, size_t, size_t row, size_t column, bool escaped);
  void addLastField(char const*, size_t, size_t row, size_t column,
                    bool escaped);

  bool checkCreateCollection();
  bool truncateCollection();

  void sendCsvBuffer();
  void sendJsonBuffer(char const* str, size_t len, bool isObject);
  SenderThread* findSender();
  void waitForSenders();

 private:
  std::unique_ptr<httpclient::SimpleHttpClient> _httpClient;
  uint64_t const _maxUploadSize;
  std::vector<std::unique_ptr<SenderThread>> _senderThreads;

  std::string _separator;
  std::string _quote;
  std::string _createCollectionType;
  bool _useBackslash;
  bool _convert;
  bool _createCollection;
  bool _overwrite;
  bool _progress;
  bool _firstChunk;

  size_t _numberLines;
  ImportStatistics _stats;

  size_t _rowsRead;
  size_t _rowOffset;
  size_t _rowsToSkip;

  int64_t _keyColumn;

  std::string _onDuplicateAction;
  std::string _collectionName;
  std::string _fromCollectionPrefix;
  std::string _toCollectionPrefix;
  arangodb::basics::StringBuffer _lineBuffer;
  arangodb::basics::StringBuffer _outputBuffer;
  std::string _firstLine;
  std::vector<std::string> _columnNames;

  std::unordered_map<std::string, std::string> _translations;
  std::unordered_set<std::string> _removeAttributes;

  bool _hasError;
  std::vector<std::string> _errorMessages;

  static double const ProgressStep;
};
}
}
#endif

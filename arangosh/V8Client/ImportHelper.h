////////////////////////////////////////////////////////////////////////////////
/// @brief import helper
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

#ifndef ARANGODB_V8CLIENT_IMPORT_HELPER_H
#define ARANGODB_V8CLIENT_IMPORT_HELPER_H 1

#include "Basics/Common.h"

#include "Basics/csv.h"
#include "Basics/StringBuffer.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

namespace triagens {
  namespace httpclient {
    class SimpleHttpClient;
    class SimpleHttpResult;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief class for http requests
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace v8client {

    class ImportHelper {
    public:

////////////////////////////////////////////////////////////////////////////////
/// @brief type of delimited import
////////////////////////////////////////////////////////////////////////////////

      enum DelimitedImportType {
        CSV = 0,
        TSV
      };

    private:
      ImportHelper (ImportHelper const&);
      ImportHelper& operator= (ImportHelper const&);

    public:
////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

      ImportHelper (httpclient::SimpleHttpClient* client, uint64_t maxUploadSize);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

      ~ImportHelper ();

////////////////////////////////////////////////////////////////////////////////
/// @brief imports a delimited file
////////////////////////////////////////////////////////////////////////////////

      bool importDelimited (std::string const& collectionName,
                            std::string const& fileName,
                            DelimitedImportType typeImport);

////////////////////////////////////////////////////////////////////////////////
/// @brief imports a file with JSON objects
/// each line must contain a complete JSON object
////////////////////////////////////////////////////////////////////////////////

      bool importJson (std::string const& collectionName, std::string const& fileName);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the quote character
///
/// this is a string because the quote might also be empty if not used
////////////////////////////////////////////////////////////////////////////////

      void setQuote (string quote) {
        _quote = quote;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not backslashes can be used for escaping quotes
////////////////////////////////////////////////////////////////////////////////

      void useBackslash (bool value) {
        _useBackslash = value;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the separator
////////////////////////////////////////////////////////////////////////////////

      void setSeparator (string separator) {
        _separator = separator;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the createCollection flag
///
/// @param bool value                create the collection if it does not exist
////////////////////////////////////////////////////////////////////////////////

      void setCreateCollection (bool value) {
        _createCollection = value;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not to overwrite existing data in the collection
////////////////////////////////////////////////////////////////////////////////

      void setOverwrite (bool value) {
        _overwrite = value;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the progress indicator
////////////////////////////////////////////////////////////////////////////////

      void setProgress (bool value) {
        _progress = value;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the number of read lines
///
/// @return size_t       number of read lines
////////////////////////////////////////////////////////////////////////////////

      size_t getReadLines () {
        return _numberLines;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the number of imported lines
///
/// @return size_t       number of imported lines
////////////////////////////////////////////////////////////////////////////////

      size_t getImportedLines () {
        return _numberOk;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the number of error lines
///
/// @return size_t       number of error lines
////////////////////////////////////////////////////////////////////////////////

      size_t getErrorLines () {
        return _numberError;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief increase the row counter
////////////////////////////////////////////////////////////////////////////////

      void incRowsRead () {
        ++_rowsRead;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the error message
///
/// @return string       get the error message
////////////////////////////////////////////////////////////////////////////////

      string getErrorMessage () {
        return _errorMessage;
      }

    private:
      static void ProcessCsvBegin (TRI_csv_parser_t*, size_t);
      static void ProcessCsvAdd (TRI_csv_parser_t*, char const*, size_t, size_t, size_t, bool);
      static void ProcessCsvEnd (TRI_csv_parser_t*, char const*, size_t, size_t, size_t, bool);

      void reportProgress (int64_t, int64_t, double&);

      std::string getCollectionUrlPart ();
      void beginLine (size_t row);
      void addField (char const*, size_t, size_t row, size_t column, bool escaped);
      void addLastField (char const*, size_t, size_t row, size_t column, bool escaped);

      void sendCsvBuffer ();
      void sendJsonBuffer (char const* str, size_t len, bool isArray);
      void handleResult (httpclient::SimpleHttpResult* result);

    private:
      httpclient::SimpleHttpClient* _client;
      uint64_t _maxUploadSize;

      std::string _separator;
      std::string _quote;
      bool _useBackslash;
      bool _createCollection;
      bool _overwrite;
      bool _progress;
      bool _firstChunk;

      size_t _numberLines;
      size_t _numberOk;
      size_t _numberError;

      size_t _rowsRead;
      size_t _rowOffset;

      std::string _collectionName;
      triagens::basics::StringBuffer _lineBuffer;
      triagens::basics::StringBuffer _outputBuffer;
      std::string _firstLine;

      bool _hasError;
      std::string _errorMessage;

      static const double ProgressStep;
    };
  }
}
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

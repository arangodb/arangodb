////////////////////////////////////////////////////////////////////////////////
/// @brief import helper
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by triAGENS GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2008-2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_V8_CLIENT_IMPORT_HELPER_H
#define TRIAGENS_V8_CLIENT_IMPORT_HELPER_H 1

#include "Basics/Common.h"

#include <regex.h>

#include <string>
#include <map>
#include <sstream>

#include <v8.h>

#include "BasicsC/csv.h"
#include "Basics/StringBuffer.h"

#ifdef _WIN32
#include "BasicsC/win-utils.h"
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

      bool importDelimited (const string& collectionName, 
                            const string& fileName, 
                            const DelimitedImportType typeImport);
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief imports a file with JSON objects
      /// each line must contain a complete JSON object
      ////////////////////////////////////////////////////////////////////////////////

      bool importJson (const string& collectionName, const string& fileName);
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief sets the quote character
      ///
      /// this is a string because the quote might also be empty if not used
      ////////////////////////////////////////////////////////////////////////////////

      void setQuote (string quote) {
        _quote = quote;
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

      void setCreateCollection (const bool value) {
        _createCollection = value;
      }
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief set the progress indicator
      ////////////////////////////////////////////////////////////////////////////////
      
      void setProgress (const bool value) {
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
      static void ProcessCsvBegin (TRI_csv_parser_t* , size_t );
      static void ProcessCsvAdd (TRI_csv_parser_t* parser, char const* field, size_t row, size_t column, bool escaped);
      static void ProcessCsvEnd (TRI_csv_parser_t* parser, char const* field, size_t row, size_t column, bool escaped);      
    
      void reportProgress (const int64_t, const int64_t, double&);
      
      string getCollectionUrlPart ();
      void beginLine (size_t row);
      void addField (char const* field, size_t row, size_t column, bool escaped);
      void addLastField (char const* field, size_t row, size_t column, bool escaped);      
      
      void sendCsvBuffer ();
      void sendJsonBuffer (char const* str, size_t len, bool isArray);
      void handleResult (httpclient::SimpleHttpResult* result);
            
    private:      
      httpclient::SimpleHttpClient* _client;
      uint64_t _maxUploadSize;
      
      string _separator;
      string _quote;

      bool _createCollection; 
      bool _progress; 
      
      size_t _numberLines;
      size_t _numberOk;
      size_t _numberError;

      size_t _rowsRead;
      size_t _rowOffset;
      
      string _collectionName;
      triagens::basics::StringBuffer _lineBuffer;
      triagens::basics::StringBuffer _outputBuffer;
      string _firstLine;
      
      regex_t _doubleRegex;
      regex_t _intRegex;
      
      bool _hasError;
      string _errorMessage;

      static const double ProgressStep;
    };
  }
}
#endif


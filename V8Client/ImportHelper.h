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

#include <string>
#include <map>
#include <sstream>

#include "regex.h"

#include <Basics/Common.h>
#include <v8.h>
#include <csv.h>

#include <Basics/StringBuffer.h>

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
    private:
      ImportHelper (ImportHelper const&);
      ImportHelper& operator= (ImportHelper const&);

    public:
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief constructor
      ///
      /// @param SimpleHttpClient* client   connection to arango server
      ///
      ////////////////////////////////////////////////////////////////////////////////

      ImportHelper (httpclient::SimpleHttpClient* client, size_t maxUploadSize);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief destructor
      ////////////////////////////////////////////////////////////////////////////////

      ~ImportHelper ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief imports a csv file
      ///
      /// @param string collectionName                name of the collection
      /// @param string fileName                      name of the csv file
      ///
      /// @return bool                                return true, if data was imported
      ////////////////////////////////////////////////////////////////////////////////

      bool importCsv (const string& collectionName, const string& fileName);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief imports a  file with JSON objects
      ///
      /// @param string collectionName                name of the collection
      /// @param string fileName                      name of the file
      ///
      /// @return bool                                return true, if data was imported
      ////////////////////////////////////////////////////////////////////////////////

      bool importJson (const string& collectionName, const string& fileName);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief sets the quote character
      ///
      /// @param char quote                the quote character
      ////////////////////////////////////////////////////////////////////////////////

      void setQuote (char quote) {
        _quote = quote;
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief sets the separator character
      ///
      /// @param char quote                the separator character
      ////////////////////////////////////////////////////////////////////////////////

      void setSeparator (char separator) {
        _separator = separator;
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
      
      void beginLine (size_t row);
      void addField (char const* field, size_t row, size_t column, bool escaped);
      void addLastField (char const* field, size_t row, size_t column, bool escaped);      
      
      void sendCsvBuffer ();
      void sendJsonBuffer (char const* str, size_t len);
      void handleResult (httpclient::SimpleHttpResult* result);
            
    private:      
      httpclient::SimpleHttpClient* _client;
      size_t _maxUploadSize;
      
      char _quote;
      char _separator;
      
      size_t _numberLines;
      size_t _numberOk;
      size_t _numberError;
      
      string _collectionName;
      triagens::basics::StringBuffer _lineBuffer;
      triagens::basics::StringBuffer _outputBuffer;
      string _firstLine;
      
      regex_t _doubleRegex;
      regex_t _intRegex;
      
      bool _hasError;
      string _errorMessage;
    };
  }
}
#endif


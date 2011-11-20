////////////////////////////////////////////////////////////////////////////////
/// @brief bison parser driver
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef FYN_JSONPARSERX_JSON_PARSER_XDRIVER_H
#define FYN_JSONPARSERX_JSON_PARSER_XDRIVER_H 1

#include <Basics/Common.h>

////////////////////////////////////////////////////////////////////////////////
// @brief flex prototyp
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace basics {
    class VariantArray;
    class VariantObject;
    class VariantVector;
  }

  namespace rest {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief parser driver
    ////////////////////////////////////////////////////////////////////////////////

    class JsonParserXDriver {
      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructor
        ////////////////////////////////////////////////////////////////////////////////

        JsonParserXDriver ();

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief destructor
        ////////////////////////////////////////////////////////////////////////////////

        ~JsonParserXDriver ();

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief parses a string
        ////////////////////////////////////////////////////////////////////////////////

        basics::VariantObject* parse (string const& scanStr);
        basics::VariantObject* parse (const char* scanStr);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the error flag
        ////////////////////////////////////////////////////////////////////////////////

        bool hasError () const {
          return error;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the last error message
        ////////////////////////////////////////////////////////////////////////////////

        string const& getError () const {
          return errorMessage;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the last error column
        ////////////////////////////////////////////////////////////////////////////////

        size_t getErrorColumn () const {
          return errorColumn;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns the last error row
        ////////////////////////////////////////////////////////////////////////////////

        size_t getErrorRow () const {
          return errorRow;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief sets the tracing attribute for scanning
        ////////////////////////////////////////////////////////////////////////////////

        void setTraceScanning (bool value) {
          traceScanning = value;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief sets the tracing attribute for parsing
        ////////////////////////////////////////////////////////////////////////////////

        void setTraceParsing (bool value) {
          traceParsing = value;
        }

      private:
        void doParse ();
        void scan_begin ();
        void scan_end ();

      public:
        void addVariantArray   (basics::VariantArray* vArray);
        void addVariantBoolean (bool tempBool);
        void addVariantDouble  (double tempDec);
        void addVariantInt32   (int32_t tempInt);
        void addVariantInt64   (int64_t tempInt);
        void addVariantNull    ();
        void addVariantString  (const string& tempStr);
        void addVariantUInt32  (uint32_t tempInt);
        void addVariantUInt64  (uint64_t tempInt);
        void addVariantVector  (basics::VariantVector* vVector);

      public:
        void setError (size_t row, size_t column, string const& m);
        void setError (string const& m);

      private:
        bool traceScanning;
        bool traceParsing;
        bool error;

        char* scanString;
        string errorMessage;
        size_t errorRow;
        size_t errorColumn;

        basics::VariantObject* json;
        bool isVariantArray;

      public:
        string buffer;   // use this buffer for flex to create strings
        void*  scanner;  // scanner storage rather than global variables for reentrant scanner
    };
  }
}

#endif

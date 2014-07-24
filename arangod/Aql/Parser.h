////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, query parser
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_PARSER_H
#define ARANGODB_AQL_PARSER_H 1

#include "Basics/Common.h"
#include "Aql/Query.h"

namespace triagens {
  namespace aql {

    class Query;

// -----------------------------------------------------------------------------
// --SECTION--                                                      class Parser
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the parser
////////////////////////////////////////////////////////////////////////////////

    class Parser {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief create the parser
////////////////////////////////////////////////////////////////////////////////

        Parser (Query*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the parser
////////////////////////////////////////////////////////////////////////////////

        ~Parser ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the scanner
////////////////////////////////////////////////////////////////////////////////
        
        inline void* scanner () const {
          return _scanner;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief a pointer to the start of the query string
////////////////////////////////////////////////////////////////////////////////
        
        inline char const* queryString () const {
          return _query->queryString();
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the remaining length of the query string to process
////////////////////////////////////////////////////////////////////////////////

        inline size_t remainingLength () const {
          return _remainingLength;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the current marker position
////////////////////////////////////////////////////////////////////////////////

        inline char const* marker () const {
          return _marker;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the current marker position
////////////////////////////////////////////////////////////////////////////////

        inline void marker (char const* marker) {
          _marker = marker;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the current parse position
////////////////////////////////////////////////////////////////////////////////
        
        inline size_t offset () const {
          return _offset;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief adjust the current parse position
////////////////////////////////////////////////////////////////////////////////

        inline void increaseOffset (int offset) {
          _offset += (size_t) offset;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief fill the output buffer with a fragment of the query
////////////////////////////////////////////////////////////////////////////////

        void fillBuffer (char* result,
                         size_t length) {
          memcpy(result, _buffer, length);
          _buffer += length;
          _remainingLength -= length;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief register a string
////////////////////////////////////////////////////////////////////////////////

        char* registerString (char const* p, 
                              size_t length,
                              bool mustEscape) {
          // TODO
          return nullptr;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the query type
////////////////////////////////////////////////////////////////////////////////

        void type (QueryType type) {
          _query->type(type);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief register a parse error
////////////////////////////////////////////////////////////////////////////////

        void registerError (char const*,
                            int,
                            int);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

        Query*        _query;           // the query object

        void*         _scanner; // the lexer generated by flex
        char const*   _buffer;          // the currently processed part of the query string
        size_t        _remainingLength; // remaining length of the query string, modified during parsing
        size_t        _offset;          // current parse position
        char const*   _marker;          // a position used temporarily during parsing
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

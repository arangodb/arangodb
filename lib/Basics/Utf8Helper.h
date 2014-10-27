////////////////////////////////////////////////////////////////////////////////
/// @brief Utf8/Utf16 Helper class
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
/// @author Frank Celler
/// @author Achim Brandt
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_UTF8HELPER_H
#define ARANGODB_BASICS_UTF8HELPER_H 1

#include "Basics/Common.h"
#include "Basics/vector.h"

#include "unicode/coll.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  class Utf8Helper
// -----------------------------------------------------------------------------

namespace triagens {
  namespace basics {

    class Utf8Helper {
        Utf8Helper (Utf8Helper const&);
        Utf8Helper& operator= (Utf8Helper const&);

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief a default helper
////////////////////////////////////////////////////////////////////////////////

        static Utf8Helper DefaultUtf8Helper;

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        Utf8Helper();

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
/// @param lang   Lowercase two-letter or three-letter ISO-639 code.
///     This parameter can instead be an ICU style C locale (e.g. "en_US")
////////////////////////////////////////////////////////////////////////////////

        Utf8Helper (std::string const& lang);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~Utf8Helper();

      public:

////////////////////////////////////////////////////////////////////////////////
///  public functions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief compare utf8 strings
/// -1 : left < right
///  0 : left == right
///  1 : left > right
////////////////////////////////////////////////////////////////////////////////

        int compareUtf8 (const char* left, const char* right);

////////////////////////////////////////////////////////////////////////////////
/// @brief compare utf16 strings
/// -1 : left < right
///  0 : left == right
///  1 : left > right
////////////////////////////////////////////////////////////////////////////////

        int compareUtf16 (const uint16_t* left, size_t leftLength, const uint16_t* right, size_t rightLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief set collator by language
/// @param lang   Lowercase two-letter or three-letter ISO-639 code.
///     This parameter can instead be an ICU style C locale (e.g. "en_US")
////////////////////////////////////////////////////////////////////////////////

        void setCollatorLanguage (std::string const& lang);

////////////////////////////////////////////////////////////////////////////////
/// @brief get collator language
////////////////////////////////////////////////////////////////////////////////

        std::string getCollatorLanguage ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get collator country
////////////////////////////////////////////////////////////////////////////////

        std::string getCollatorCountry ();

////////////////////////////////////////////////////////////////////////////////
/// @brief Lowercase the characters in a UTF-8 string.
////////////////////////////////////////////////////////////////////////////////

        std::string toLowerCase (std::string const& src);

////////////////////////////////////////////////////////////////////////////////
/// @brief Lowercase the characters in a UTF-8 string.
////////////////////////////////////////////////////////////////////////////////

        char* tolower (TRI_memory_zone_t* zone, const char *src, int32_t srcLength, int32_t& dstLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief Uppercase the characters in a UTF-8 string.
////////////////////////////////////////////////////////////////////////////////

        std::string toUpperCase (std::string const& src);

////////////////////////////////////////////////////////////////////////////////
/// @brief Uppercase the characters in a UTF-8 string.
////////////////////////////////////////////////////////////////////////////////

        char* toupper (TRI_memory_zone_t* zone, const char *src, int32_t srcLength, int32_t& dstLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the words of a UTF-8 string.
////////////////////////////////////////////////////////////////////////////////

        TRI_vector_string_t* getWords (const char* const text,
                                                   const size_t textLength,
                                                   const size_t minimalWordLength,
                                                   const size_t maximalWordLength,
                                                   bool lowerCase);

      private:
        Collator* _coll;
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

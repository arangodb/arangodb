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
#include "unicode/ustring.h"

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
/// @param lang   Lowercase two-letter or three-letter ISO-639 code.
///     This parameter can instead be an ICU style C locale (e.g. "en_US")
////////////////////////////////////////////////////////////////////////////////

        explicit Utf8Helper (std::string const& lang);

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        Utf8Helper();

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

        int compareUtf8 (char const* left, 
                         char const* right) const;
        
        int compareUtf8 (char const* left, 
                         size_t leftLength,
                         char const* right,
                         size_t rightLength) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief compare utf16 strings
/// -1 : left < right
///  0 : left == right
///  1 : left > right
////////////////////////////////////////////////////////////////////////////////

        int compareUtf16 (const uint16_t* left, size_t leftLength, const uint16_t* right, size_t rightLength) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief set collator by language
/// @param lang   Lowercase two-letter or three-letter ISO-639 code.
///     This parameter can instead be an ICU style C locale (e.g. "en_US")
////////////////////////////////////////////////////////////////////////////////

        bool setCollatorLanguage (std::string const& lang);

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

        char* tolower (TRI_memory_zone_t* zone, 
                       char const* src, 
                       int32_t srcLength, 
                       int32_t& dstLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief Uppercase the characters in a UTF-8 string.
////////////////////////////////////////////////////////////////////////////////

        std::string toUpperCase (std::string const& src);

////////////////////////////////////////////////////////////////////////////////
/// @brief Uppercase the characters in a UTF-8 string.
////////////////////////////////////////////////////////////////////////////////

        char* toupper (TRI_memory_zone_t* zone, 
                       char const* src, 
                       int32_t srcLength, 
                       int32_t& dstLength);
 
////////////////////////////////////////////////////////////////////////////////
/// @brief returns the words of a UTF-8 string.
////////////////////////////////////////////////////////////////////////////////

        TRI_vector_string_t* getWords (char const* text,
                                       size_t textLength,
                                       size_t minimalWordLength,
                                       size_t maximalWordLength,
                                       bool lowerCase);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the words of a UTF-8 string.
////////////////////////////////////////////////////////////////////////////////

        bool getWords (TRI_vector_string_t*& words,
                       char const* text,
                       size_t textLength,
                       size_t minimalWordLength,
                       size_t maximalWordLength,
                       bool lowerCase);

      private:
        Collator* _coll;
    };

  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                        public non-class functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a utf-8 string to a uchar (utf-16)
////////////////////////////////////////////////////////////////////////////////

UChar* TRI_Utf8ToUChar (TRI_memory_zone_t* zone,
                        char const* utf8,
                        size_t inLength,
                        size_t* outLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uchar (utf-16) to a utf-8 string
////////////////////////////////////////////////////////////////////////////////

char* TRI_UCharToUtf8 (TRI_memory_zone_t* zone,
                       UChar const* uchar,
                       size_t inLength,
                       size_t* outLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize an utf8 string (NFC)
////////////////////////////////////////////////////////////////////////////////

char* TRI_normalize_utf8_to_NFC (TRI_memory_zone_t* zone,
                                 char const* utf8,
                                 size_t inLength,
                                 size_t* outLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize an utf16 string (NFC) and export it to utf8
////////////////////////////////////////////////////////////////////////////////

char * TRI_normalize_utf16_to_NFC (TRI_memory_zone_t* zone,
                                   uint16_t const* utf16,
                                   size_t inLength,
                                   size_t* outLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two utf16 strings (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

int TRI_compare_utf16 (uint16_t const* left,
                       size_t leftLength,
                       uint16_t const* right,
                       size_t rightLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two utf8 strings (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

int TRI_compare_utf8 (char const* left, 
                      char const* right);

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two utf8 strings (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

int TRI_compare_utf8 (char const* left, 
                      size_t leftLength, 
                      char const* right, 
                      size_t rightLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief Lowercase the characters in a UTF-8 string (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

char* TRI_tolower_utf8 (TRI_memory_zone_t* zone,
                        const char *src,
                        int32_t srcLength,
                        int32_t* dstLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief Uppercase the characters in a UTF-8 string (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

char* TRI_toupper_utf8 (TRI_memory_zone_t* zone,
                        const char *src,
                        int32_t srcLength,
                        int32_t* dstLength);

////////////////////////////////////////////////////////////////////////////////
/// @brief Get words of an UTF-8 string (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

TRI_vector_string_t* TRI_get_words (char const* text,
                                    size_t textLength,
                                    size_t minimalWordLength,
                                    size_t maximalWordLength,
                                    bool lowerCase);

////////////////////////////////////////////////////////////////////////////////
/// @brief Get words of an UTF-8 string (implemented in Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

bool TRI_get_words (TRI_vector_string_t*& words,
                    char const* text,
                    size_t textLength,
                    size_t minimalWordLength,
                    size_t maximalWordLength,
                    bool lowerCase);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

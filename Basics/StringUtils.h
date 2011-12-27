////////////////////////////////////////////////////////////////////////////////
/// @brief collection of string utility functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2011 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2005-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_JUTLAND_BASICS_STRING_UTILS_H
#define TRIAGENS_JUTLAND_BASICS_STRING_UTILS_H 1

#include <Basics/Common.h>

namespace triagens {
  namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief collection of string utility functions
///
/// This namespace holds function used for string manipulation.
////////////////////////////////////////////////////////////////////////////////

    namespace StringUtils {

// -----------------------------------------------------------------------------
      // STRING AND STRING POINTER
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief creates a blob using new
////////////////////////////////////////////////////////////////////////////////

      blob_t duplicateBlob (const blob_t&);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief creates a blob using new
////////////////////////////////////////////////////////////////////////////////

      blob_t duplicateBlob (char const*, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief creates a blob using new
////////////////////////////////////////////////////////////////////////////////

      blob_t duplicateBlob (string const&);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief creates a C string using new
////////////////////////////////////////////////////////////////////////////////

      char* duplicate (string const&);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief creates a C string using new
////////////////////////////////////////////////////////////////////////////////

      char* duplicate (char const*, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief creates a C string using new
////////////////////////////////////////////////////////////////////////////////

      char* duplicate (char const*);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief deletes and clears a string
///
/// The string is cleared using memset and then deleted. The pointer is
/// set to 0.
////////////////////////////////////////////////////////////////////////////////

      void destroy (char*&);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief deletes and clears a string
///
/// The string is cleared using memset and then deleted. The pointer is
/// set to 0.
////////////////////////////////////////////////////////////////////////////////

      void destroy (char*&, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief deletes and clears a string
///
/// The string is cleared using memset and then deleted. The pointer is
/// set to 0.
////////////////////////////////////////////////////////////////////////////////

      void destroy (blob_t&);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief deletes but does not clear a character string
///
/// The pointer deleted and then set to 0.
////////////////////////////////////////////////////////////////////////////////

      void erase (char*&);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief deletes but does not clear a character string with a given size
///
/// The pointer deleted and then set to 0.
////////////////////////////////////////////////////////////////////////////////

      void erase (char*&, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief deletes but does not  clear a blob
///
/// The data pointer deleted and then set to 0, and the length is set to 0.
////////////////////////////////////////////////////////////////////////////////

      void erase (blob_t&);

// -----------------------------------------------------------------------------
      // STRING CONVERSION
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief capitalize string
///
/// This method converts characters at the beginning of a word to uppercase
/// and remove any whitespaces. If first is true the first character of the
/// first word is also converted to uppercase. Name must not be empty.
////////////////////////////////////////////////////////////////////////////////

      string capitalize (string const& name, bool first = true);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief separate words
///
/// This method converts all characters to lowercase and separates
/// the words with a given character. Name must not be empty.
////////////////////////////////////////////////////////////////////////////////

      string separate (string const& name, char separator = '-');

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief escape delimiter
///
/// This method escapes a set of character with a given escape character. The
/// escape character is also escaped.
////////////////////////////////////////////////////////////////////////////////

      string escape (string const& name, string const& specials, char quote = '\\');

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief escape delimiter
///
/// This method escapes a set of character with a given escape character. The
/// escape character is also escaped.
////////////////////////////////////////////////////////////////////////////////

      string escape (string const& name, size_t len, string const& specials, char quote = '\\');

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief escape unicode
///
/// This method escapes a unicode character string by replacing the unicode
/// characters by a \uXXXX sequence.
////////////////////////////////////////////////////////////////////////////////

      string escapeUnicode (string const& name, bool escapeSlash = true);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief escape html
////////////////////////////////////////////////////////////////////////////////

      string escapeHtml (string const& name);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief escape xml
////////////////////////////////////////////////////////////////////////////////

      string escapeXml (string const& name);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief escape hex for all non-printable characters (including space)
////////////////////////////////////////////////////////////////////////////////

      string escapeHex (string const& name, char quote = '%');

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief escape hex
////////////////////////////////////////////////////////////////////////////////

      string escapeHex (string const& name, string const& specials, char quote = '%');

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief escape as C code
////////////////////////////////////////////////////////////////////////////////

      string escapeC (string const& name);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief splits a string
////////////////////////////////////////////////////////////////////////////////

      vector<string> split (string const& source, char delim = ',', char quote = '\\');

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief splits a string
////////////////////////////////////////////////////////////////////////////////

      vector<string> split (string const& source, string const& delim, char quote = '\\');

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief joins a string
////////////////////////////////////////////////////////////////////////////////

      string join (vector<string> const& source, char delim = ',');

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief joins a string
////////////////////////////////////////////////////////////////////////////////

      string join (vector<string> const& source, string const& delim = ",");

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief joins a string
////////////////////////////////////////////////////////////////////////////////

      string join (set<string> const& source, char delim = ',');

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief joins a string
////////////////////////////////////////////////////////////////////////////////

      string join (set<string> const& source, string const& delim = ",");

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief removes leading and trailing whitespace
////////////////////////////////////////////////////////////////////////////////

      string trim (string const& sourceStr, string const& trimStr = " \t\n\r");

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief removes leading and trailing whitespace in place
////////////////////////////////////////////////////////////////////////////////

      void trimInPlace (string& str, string const& trimStr = " \t\n\r");

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief removes leading whitespace
////////////////////////////////////////////////////////////////////////////////

      string lTrim(string const& sourceStr, string const& trimStr = " \t\n\r");

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief removes trailing whitespace
////////////////////////////////////////////////////////////////////////////////

      string rTrim(string const& sourceStr, string const& trimStr = " \t\n\r");

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief fills string from left
////////////////////////////////////////////////////////////////////////////////

      string lFill(string const& sourceStr, size_t size, char fill = ' ');

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief fills string from right
////////////////////////////////////////////////////////////////////////////////

      string rFill(string const& sourceStr, size_t size, char fill = ' ');

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief wrap longs lines
////////////////////////////////////////////////////////////////////////////////

      vector<string> wrap(string const& sourceStr, size_t size, string breaks = " ");

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief substring replace
////////////////////////////////////////////////////////////////////////////////

      string replace (string const& sourceStr, string const& fromString, string const& toString);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts string to lower case
////////////////////////////////////////////////////////////////////////////////

      string tolower (string const& str);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts string to upper case
////////////////////////////////////////////////////////////////////////////////

      string toupper (string const& str);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief checks for a prefix
////////////////////////////////////////////////////////////////////////////////

      bool isPrefix (string const& str, string const& prefix);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief checks for a suffix
////////////////////////////////////////////////////////////////////////////////

      bool isSuffix (string const& str, string const& postfix);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief url decodes the string
////////////////////////////////////////////////////////////////////////////////

      string urlDecode (string const& str);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief url encodes the string
////////////////////////////////////////////////////////////////////////////////

      string urlEncode (const char* src);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief url encodes the string
////////////////////////////////////////////////////////////////////////////////

      string urlEncode (const char* src, const size_t len);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief unicode hexidecmial characters to utf8
////////////////////////////////////////////////////////////////////////////////

      bool unicodeToUTF8 (const char* inputStr, const size_t& len, string& outputStr);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts an utf16 symbol which needs UTF16 to UTF8
///        The conversion correspond to the specification:
///        http://en.wikipedia.org/wiki/UTF-16#Code_points_U.2B10000_to_U.2B10FFFF
////////////////////////////////////////////////////////////////////////////////

      bool convertUTF16ToUTF8 (const char* high_surrogate, const char* low_surrogate, string& outputStr);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief url encodes the string
////////////////////////////////////////////////////////////////////////////////

      string urlEncode (string const& str);

// -----------------------------------------------------------------------------
      // CONVERT TO STRING
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts integer to string
////////////////////////////////////////////////////////////////////////////////

      string itoa (int16_t i);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts unsigned integer to string
////////////////////////////////////////////////////////////////////////////////

      string itoa (uint16_t i);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts integer to string
////////////////////////////////////////////////////////////////////////////////

      string itoa (int64_t i);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts unsigned integer to string
////////////////////////////////////////////////////////////////////////////////

      string itoa (uint64_t i);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts integer to string
////////////////////////////////////////////////////////////////////////////////

      string itoa (int32_t i);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts unsigned integer to string
////////////////////////////////////////////////////////////////////////////////

      string itoa (uint32_t i);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts size_t to string
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_OVERLOAD_FUNCS_SIZE_T

#if TRI_SIZEOF_SIZE_T == 4

      inline string itoa (size_t i) {
        return itoa(uint32_t(i));
      }

#elif TRI_SIZEOF_SIZE_T == 8

      inline string itoa (size_t i) {
        return itoa(uint64_t(i));
      }

#endif

#endif

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts decimal to string
////////////////////////////////////////////////////////////////////////////////

      string ftoa (double i);

// -----------------------------------------------------------------------------
      // CONVERT FROM STRING
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts a single hex to integer
////////////////////////////////////////////////////////////////////////////////

      inline int hex2int (char ch, int errorValue = 0) {
        if ('0' <= ch && ch <= '9') {
          return ch - '0';
        }
        else if ('A' <= ch && ch <= 'F') {
          return ch - 'A' + 10;
        }
        else if ('a' <= ch && ch <= 'f') {
          return ch - 'a' + 10;
        }

        return errorValue;
      }

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses a boolean
////////////////////////////////////////////////////////////////////////////////

      bool boolean (string const& str);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses an integer
////////////////////////////////////////////////////////////////////////////////

      int64_t int64 (string const& str);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses an integer
////////////////////////////////////////////////////////////////////////////////

      int64_t int64 (char const* value, size_t size);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses an unsigned integer
////////////////////////////////////////////////////////////////////////////////

      uint64_t uint64 (string const& str);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses an unsigned integer
////////////////////////////////////////////////////////////////////////////////

      uint64_t uint64 (char const* value, size_t size);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses an integer
////////////////////////////////////////////////////////////////////////////////

      int32_t int32 (string const& str);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses an integer
////////////////////////////////////////////////////////////////////////////////

      int32_t int32 (char const* value, size_t size);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses an unsigned integer
////////////////////////////////////////////////////////////////////////////////

      uint32_t uint32 (string const& str);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses an unsigned integer
////////////////////////////////////////////////////////////////////////////////

      uint32_t uint32 (char const* value, size_t size);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses an unsigned integer given in HEX
////////////////////////////////////////////////////////////////////////////////

      uint32_t unhexUint32 (string const& str);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses an unsigned integer given in HEX
////////////////////////////////////////////////////////////////////////////////

      uint32_t unhexUint32 (char const* value, size_t size);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses a decimal
////////////////////////////////////////////////////////////////////////////////

      double doubleDecimal (string const& str);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses a decimal
////////////////////////////////////////////////////////////////////////////////

      double doubleDecimal (char const* value, size_t size);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses a decimal
////////////////////////////////////////////////////////////////////////////////

      float floatDecimal (string const& str);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses a decimal
////////////////////////////////////////////////////////////////////////////////

      float floatDecimal (char const* value, size_t size);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses a time
////////////////////////////////////////////////////////////////////////////////

      seconds_t seconds (string const& format, string const& str);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief formats a time using the extended ISO 8601 format
////////////////////////////////////////////////////////////////////////////////

      string formatSeconds (seconds_t);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief formats a time using the oracle format string
////////////////////////////////////////////////////////////////////////////////

      string formatSeconds (string const& format, seconds_t);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses a date
////////////////////////////////////////////////////////////////////////////////

      date_t date (string const& format, string const& str);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief formats a date using the extended ISO 8601 format
////////////////////////////////////////////////////////////////////////////////

      string formatDate (date_t);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief formats a date using the oracle format string
////////////////////////////////////////////////////////////////////////////////

      string formatDate (string const& format, date_t);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief parses a date and time
////////////////////////////////////////////////////////////////////////////////

      datetime_t datetime (string const& format, string const& str);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief formats a datetime using the extended ISO 8601 format
////////////////////////////////////////////////////////////////////////////////

      string formatDatetime (datetime_t);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief formats a datetime using the oracle format string
////////////////////////////////////////////////////////////////////////////////

      string formatDatetime (string const& format, datetime_t);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief formats a date and time using the extended ISO 8601 format
////////////////////////////////////////////////////////////////////////////////

      string formatDateTime (date_t, seconds_t);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief formats a date and time using the oracle format string
////////////////////////////////////////////////////////////////////////////////

      string formatDateTime (string const& format, date_t, seconds_t);

// -----------------------------------------------------------------------------
      // BASE64
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts to base64
////////////////////////////////////////////////////////////////////////////////

      string encodeBase64 (string const&);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts from base64
////////////////////////////////////////////////////////////////////////////////

      string decodeBase64 (string const&);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts to base64, URL friendly
///
/// '-' and '_' are used instead of '+' and '/'
////////////////////////////////////////////////////////////////////////////////

      string encodeBase64U (string const&);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts from base64, URL friendly
///
/// '-' and '_' are used instead of '+' and '/'
////////////////////////////////////////////////////////////////////////////////

      string decodeBase64U (string const&);

// -----------------------------------------------------------------------------
      // ADDITIONAL STRING UTILITIES
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief finds n.th entry
////////////////////////////////////////////////////////////////////////////////

      string entry (const size_t pos, string const& sourceStr, string const& delimiter = ",");

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief counts number of entires
////////////////////////////////////////////////////////////////////////////////

      size_t numEntries(string const& sourceStr, string const& delimiter = ",");

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts to hex
////////////////////////////////////////////////////////////////////////////////

      string encodeHex (string const& str);

////////////////////////////////////////////////////////////////////////////////
/// @ingroup Utilities
/// @brief converts from hex
////////////////////////////////////////////////////////////////////////////////

      string decodeHex (string const& str);
    }
  }
}

#endif

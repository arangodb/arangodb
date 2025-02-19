////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stddef.h>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <string_view>

#include <unicode/coll.h>
#include <unicode/umachine.h>
#include <unicode/regex.h>

namespace arangodb::basics {

enum class LanguageType { INVALID, DEFAULT, ICU };

class Utf8Helper {
  Utf8Helper(Utf8Helper const&) = delete;
  Utf8Helper& operator=(Utf8Helper const&) = delete;

 public:
  // The following singleton is used in various places, in particular
  // in the LanguageFeature. When it is constructed before main(), we
  // do not yet have the ICU data available, so we cannot really initialize
  // it and set a collator language. This is then done in the `LanguageFeature`
  // in its `prepare` method. Therefore we do not initialize the collator
  // in the constructor! Do not use this singleton before the LanguageFeature
  // has initialized it.
  static Utf8Helper DefaultUtf8Helper;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructor
  //////////////////////////////////////////////////////////////////////////////

  Utf8Helper() = delete;     // avoid accidental use
  explicit Utf8Helper(int);  // the int is just to distinguish this constructor
                             // and to use it for DefaultUtf8Helper above.

  ~Utf8Helper();

  //////////////////////////////////////////////////////////////////////////////
  ///  public functions
  //////////////////////////////////////////////////////////////////////////////

  //////////////////////////////////////////////////////////////////////////////
  /// @brief compare utf8 strings
  /// -1 : left < right
  ///  0 : left == right
  ///  1 : left > right
  //////////////////////////////////////////////////////////////////////////////

  int compareUtf8(char const* left, size_t leftLength, char const* right,
                  size_t rightLength) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief compare utf16 strings
  /// -1 : left < right
  ///  0 : left == right
  ///  1 : left > right
  //////////////////////////////////////////////////////////////////////////////

  int compareUtf16(uint16_t const* left, size_t leftLength,
                   uint16_t const* right, size_t rightLength) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set collator by language
  /// @param lang   Lowercase two-letter or three-letter ISO-639 code.
  ///     This parameter can instead be an ICU style C locale (e.g. "en_US")
  /// @param icuDataPointer data file to be loaded by the application
  /// @param langType type of language. Now supports DEFAULT and ICU only
  /// collation
  //////////////////////////////////////////////////////////////////////////////

  bool setCollatorLanguage(std::string_view lang, LanguageType langType /*,
                           void* icuDataPointer*/);

#ifdef ARANGODB_USE_GOOGLE_TESTS
  //////////////////////////////////////////////////////////////////////////////
  /// @brief get current collator
  //////////////////////////////////////////////////////////////////////////////

  icu_64_64::Collator* getCollator() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set collator
  //////////////////////////////////////////////////////////////////////////////

  void setCollator(std::unique_ptr<icu_64_64::Collator> coll);
#endif

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get collator language
  //////////////////////////////////////////////////////////////////////////////

  std::string getCollatorLanguage();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get collator country
  //////////////////////////////////////////////////////////////////////////////

  std::string getCollatorCountry();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Lowercase the characters in a UTF-8 string.
  //////////////////////////////////////////////////////////////////////////////

  std::string toLowerCase(std::string const& src);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Lowercase the characters in a UTF-8 string.
  //////////////////////////////////////////////////////////////////////////////

  char* tolower(char const* src, int32_t srcLength, int32_t& dstLength);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Uppercase the characters in a UTF-8 string.
  //////////////////////////////////////////////////////////////////////////////

  std::string toUpperCase(std::string const& src);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Uppercase the characters in a UTF-8 string.
  //////////////////////////////////////////////////////////////////////////////

  char* toupper(char const* src, int32_t srcLength, int32_t& dstLength);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the words of a UTF-8 string.
  //////////////////////////////////////////////////////////////////////////////

  bool tokenize(std::set<std::string>& words, std::string_view text,
                size_t minimalWordLength, size_t maximalWordLength,
                bool lowerCase);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief builds a regex matcher for the specified pattern
  //////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<icu_64_64::RegexMatcher> buildMatcher(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not value matches a regex
  //////////////////////////////////////////////////////////////////////////////

  bool matches(icu_64_64::RegexMatcher*, char const* pattern,
               size_t patternLength, bool partial, bool& error);

  std::string replace(icu_64_64::RegexMatcher*, char const* pattern,
                      size_t patternLength, char const* replacement,
                      size_t replacementLength, bool partial, bool& error);

  // append an UTF8 to a string. This will append 1 to 4 bytes.
  static void appendUtf8Character(std::string& result, uint32_t ch) {
    if (ch <= 0x7f) {
      result.push_back((uint8_t)ch);
    } else {
      if (ch <= 0x7ff) {
        result.push_back((uint8_t)((ch >> 6) | 0xc0));
      } else {
        if (ch <= 0xffff) {
          result.push_back((uint8_t)((ch >> 12) | 0xe0));
        } else {
          result.push_back((uint8_t)((ch >> 18) | 0xf0));
          result.push_back((uint8_t)(((ch >> 12) & 0x3f) | 0x80));
        }
        result.push_back((uint8_t)(((ch >> 6) & 0x3f) | 0x80));
      }
      result.push_back((uint8_t)((ch & 0x3f) | 0x80));
    }
  }

 private:
  std::unique_ptr<icu_64_64::Collator> _coll;
};

}  // namespace arangodb::basics

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a utf-8 string to a uchar (utf-16)
////////////////////////////////////////////////////////////////////////////////

UChar* TRI_Utf8ToUChar(char const* utf8, size_t inLength, size_t* outLength,
                       UErrorCode* status = nullptr);
UChar* TRI_Utf8ToUChar(char const* utf8, size_t inLength, UChar* buffer,
                       size_t bufferSize, size_t* outLength,
                       UErrorCode* status = nullptr);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uchar (utf-16) to a utf-8 string
////////////////////////////////////////////////////////////////////////////////

char* TRI_UCharToUtf8(UChar const* uchar, size_t inLength, size_t* outLength,
                      UErrorCode* status = nullptr);

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize an utf8 string (NFC)
////////////////////////////////////////////////////////////////////////////////

char* TRI_normalize_utf8_to_NFC(char const* utf8, size_t inLength,
                                size_t* outLength,
                                UErrorCode* status = nullptr);

std::string normalizeUtf8ToNFC(std::string_view value);

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize an utf16 string (NFC) and export it to utf8
////////////////////////////////////////////////////////////////////////////////

char* TRI_normalize_utf16_to_NFC(uint16_t const* utf16, size_t inLength,
                                 size_t* outLength,
                                 UErrorCode* status = nullptr);

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two utf8 strings
////////////////////////////////////////////////////////////////////////////////

static inline int TRI_compare_utf8(char const* left, size_t leftLength,
                                   char const* right, size_t rightLength) {
  return arangodb::basics::Utf8Helper::DefaultUtf8Helper.compareUtf8(
      left, leftLength, right, rightLength);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Lowercase the characters in a UTF-8 string
////////////////////////////////////////////////////////////////////////////////

char* TRI_tolower_utf8(char const* src, int32_t srcLength, int32_t* dstLength);

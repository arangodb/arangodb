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
/// @author Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "Utf8Helper.h"
#include "Logger/Logger.h"
#include "Basics/StaticStrings.h"
#include "Basics/directories.h"
#include "Basics/tri-strings.h"
#include "unicode/normalizer2.h"
#include "unicode/brkiter.h"
#include "unicode/ucasemap.h"
#include "unicode/uclean.h"
#include "unicode/unorm2.h"
#include "unicode/ustdio.h"
#include "unicode/putil.h"
#include "unicode/udata.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

using namespace arangodb::basics;

Utf8Helper Utf8Helper::DefaultUtf8Helper(nullptr);

Utf8Helper::Utf8Helper(std::string const& lang, void *icuDataPtr)
  : _coll(nullptr) {
  setCollatorLanguage(lang, icuDataPtr);
}

Utf8Helper::Utf8Helper(void *icuDataPtr) : Utf8Helper("", icuDataPtr) {}

Utf8Helper::~Utf8Helper() {
  if (_coll) {
    delete _coll;
    if (this == &DefaultUtf8Helper) {
      u_cleanup();
    }
  }
}

int Utf8Helper::compareUtf8(char const* left, char const* right) const {
  TRI_ASSERT(left != nullptr);
  TRI_ASSERT(right != nullptr);

  if (!_coll) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "no Collator in Utf8Helper::compareUtf8()!";
    return (strcmp(left, right));
  }

  UErrorCode status = U_ZERO_ERROR;
  int result =
      _coll->compareUTF8(StringPiece(left), StringPiece(right), status);
  if (U_FAILURE(status)) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error in Collator::compareUTF8(...): " << u_errorName(status);
    return (strcmp(left, right));
  }

  return result;
}

int Utf8Helper::compareUtf8(char const* left, size_t leftLength,
                            char const* right, size_t rightLength) const {
  TRI_ASSERT(left != nullptr);
  TRI_ASSERT(right != nullptr);
  TRI_ASSERT(_coll);

  UErrorCode status = U_ZERO_ERROR;
  int result =
      _coll->compareUTF8(StringPiece(left, (int32_t)leftLength),
                         StringPiece(right, (int32_t)rightLength), status);
  if (U_FAILURE(status)) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error in Collator::compareUTF8(...): " << u_errorName(status);
    return (strncmp(left, right, leftLength < rightLength ? leftLength : rightLength));
  }

  return result;
}

int Utf8Helper::compareUtf16(uint16_t const* left, size_t leftLength,
                             uint16_t const* right, size_t rightLength) const {
  TRI_ASSERT(left != nullptr);
  TRI_ASSERT(right != nullptr);

  if (!_coll) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "no Collator in Utf8Helper::compareUtf16()!";

    if (leftLength == rightLength) {
      return memcmp((const void*)left, (const void*)right, leftLength * 2);
    }

    int result =
        memcmp((const void*)left, (const void*)right,
               leftLength < rightLength ? leftLength * 2 : rightLength * 2);

    if (result == 0) {
      if (leftLength < rightLength) {
        return -1;
      }
      return 1;
    }

    return result;
  }
  // ..........................................................................
  // Take note here: we are assuming that the ICU type UChar is two bytes.
  // There is no guarantee that this will be the case on all platforms and
  // compilers.
  // ..........................................................................
  return _coll->compare((const UChar*)left, (int32_t)leftLength,
                        (const UChar*)right, (int32_t)rightLength);
}

bool Utf8Helper::setCollatorLanguage(std::string const& lang, void* icuDataPointer) {
  if (icuDataPointer == nullptr) {
     return false;
    }

  UErrorCode status = U_ZERO_ERROR;
  udata_setCommonData(reinterpret_cast<void*>(icuDataPointer), &status);
  if (U_FAILURE(status)) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error while udata_setCommonData(...): " << u_errorName(status);
    return false;
  }
  status = U_ZERO_ERROR;

  if (_coll) {
    ULocDataLocaleType type = ULOC_ACTUAL_LOCALE;
    const Locale& locale = _coll->getLocale(type, status);

    if (U_FAILURE(status)) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error in Collator::getLocale(...): " << u_errorName(status);
      return false;
    }
    if (lang == locale.getName()) {
      return true;
    }
  }

  Collator* coll;
  if (lang == "") {
    // get default collator for empty language
    coll = Collator::createInstance(status);
  } else {
    Locale locale(lang.c_str());
    coll = Collator::createInstance(locale, status);
  }

  if (U_FAILURE(status)) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error in Collator::createInstance('" << lang << "'): " << u_errorName(status);
    if (coll) {
      delete coll;
    }
    return false;
  }

  // set the default attributes for sorting:
  coll->setAttribute(UCOL_CASE_FIRST, UCOL_UPPER_FIRST, status);  // A < a
  coll->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_OFF,
                     status);  // no normalization
  coll->setAttribute(
      UCOL_STRENGTH, UCOL_IDENTICAL,
      status);  // UCOL_IDENTICAL, UCOL_PRIMARY, UCOL_SECONDARY, UCOL_TERTIARY

  if (U_FAILURE(status)) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error in Collator::setAttribute(...): " << u_errorName(status);
    delete coll;
    return false;
  }

  if (_coll) {
    delete _coll;
  }

  _coll = coll;
  return true;
}

std::string Utf8Helper::getCollatorLanguage() {
  if (_coll) {
    UErrorCode status = U_ZERO_ERROR;
    ULocDataLocaleType type = ULOC_VALID_LOCALE;
    const Locale& locale = _coll->getLocale(type, status);

    if (U_FAILURE(status)) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error in Collator::getLocale(...): " << u_errorName(status);
      return "";
    }
    return locale.getLanguage();
  }
  return "";
}

std::string Utf8Helper::getCollatorCountry() {
  if (_coll) {
    UErrorCode status = U_ZERO_ERROR;
    ULocDataLocaleType type = ULOC_VALID_LOCALE;
    const Locale& locale = _coll->getLocale(type, status);

    if (U_FAILURE(status)) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error in Collator::getLocale(...): " << u_errorName(status);
      return "";
    }
    return locale.getCountry();
  }
  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Lowercase the characters in a UTF-8 string.
////////////////////////////////////////////////////////////////////////////////

std::string Utf8Helper::toLowerCase(std::string const& src) {
  int32_t utf8len = 0;
  char* utf8 = tolower(TRI_UNKNOWN_MEM_ZONE, src.c_str(), (int32_t)src.length(),
                       utf8len);

  if (utf8 == nullptr) {
    return std::string();
  }

  std::string result(utf8, utf8len);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, utf8);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Lowercase the characters in a UTF-8 string.
////////////////////////////////////////////////////////////////////////////////

char* Utf8Helper::tolower(TRI_memory_zone_t* zone, char const* src,
                          int32_t srcLength, int32_t& dstLength) {
  char* utf8_dest = nullptr;

  if (src == nullptr || srcLength == 0) {
    utf8_dest = (char*)TRI_Allocate(zone, sizeof(char));
    if (utf8_dest != nullptr) {
      utf8_dest[0] = '\0';
    }
    dstLength = 0;
    return utf8_dest;
  }

  uint32_t options = U_FOLD_CASE_DEFAULT;
  UErrorCode status = U_ZERO_ERROR;

  std::string locale = getCollatorLanguage();
  LocalUCaseMapPointer csm(ucasemap_open(locale.c_str(), options, &status));

  if (U_FAILURE(status)) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error in ucasemap_open(...): " << u_errorName(status);
  } else {
    utf8_dest =
        (char*)TRI_Allocate(zone, (srcLength + 1) * sizeof(char));
    if (utf8_dest == nullptr) {
      return nullptr;
    }

    dstLength = ucasemap_utf8ToLower(csm.getAlias(), utf8_dest, srcLength + 1,
                                     src, srcLength, &status);

    if (status == U_BUFFER_OVERFLOW_ERROR) {
      status = U_ZERO_ERROR;
      TRI_Free(zone, utf8_dest);
      utf8_dest =
          (char*)TRI_Allocate(zone, (dstLength + 1) * sizeof(char));
      if (utf8_dest == nullptr) {
        return nullptr;
      }

      dstLength = ucasemap_utf8ToLower(csm.getAlias(), utf8_dest, dstLength + 1,
                                       src, srcLength, &status);
    }

    if (U_FAILURE(status)) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error in ucasemap_utf8ToLower(...): " << u_errorName(status);
      TRI_Free(zone, utf8_dest);
    } else {
      return utf8_dest;
    }
  }

  utf8_dest = TRI_LowerAsciiString(zone, src);

  if (utf8_dest != nullptr) {
    dstLength = (int32_t)strlen(utf8_dest);
  }
  return utf8_dest;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Uppercase the characters in a UTF-8 string.
////////////////////////////////////////////////////////////////////////////////

std::string Utf8Helper::toUpperCase(std::string const& src) {
  int32_t utf8len = 0;
  char* utf8 = toupper(TRI_UNKNOWN_MEM_ZONE, src.c_str(), (int32_t)src.length(),
                       utf8len);
  if (utf8 == nullptr) {
    return std::string();
  }

  std::string result(utf8, utf8len);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, utf8);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Lowercase the characters in a UTF-8 string.
////////////////////////////////////////////////////////////////////////////////

char* Utf8Helper::toupper(TRI_memory_zone_t* zone, char const* src,
                          int32_t srcLength, int32_t& dstLength) {
  char* utf8_dest = nullptr;

  if (src == nullptr || srcLength == 0) {
    utf8_dest = (char*)TRI_Allocate(zone, sizeof(char));
    if (utf8_dest != nullptr) {
      utf8_dest[0] = '\0';
    }
    dstLength = 0;
    return utf8_dest;
  }

  uint32_t options = U_FOLD_CASE_DEFAULT;
  UErrorCode status = U_ZERO_ERROR;

  std::string locale = getCollatorLanguage();
  LocalUCaseMapPointer csm(ucasemap_open(locale.c_str(), options, &status));

  if (U_FAILURE(status)) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error in ucasemap_open(...): " << u_errorName(status);
  } else {
    utf8_dest =
        (char*)TRI_Allocate(zone, (srcLength + 1) * sizeof(char));
    if (utf8_dest == nullptr) {
      return nullptr;
    }

    dstLength = ucasemap_utf8ToUpper(csm.getAlias(), utf8_dest, srcLength, src,
                                     srcLength, &status);

    if (status == U_BUFFER_OVERFLOW_ERROR) {
      status = U_ZERO_ERROR;
      TRI_Free(zone, utf8_dest);
      utf8_dest =
          (char*)TRI_Allocate(zone, (dstLength + 1) * sizeof(char));
      if (utf8_dest == nullptr) {
        return nullptr;
      }

      dstLength = ucasemap_utf8ToUpper(csm.getAlias(), utf8_dest, dstLength + 1,
                                       src, srcLength, &status);
    }

    if (U_FAILURE(status)) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error in ucasemap_utf8ToUpper(...): " << u_errorName(status);
      TRI_Free(zone, utf8_dest);
    } else {
      return utf8_dest;
    }
  }

  utf8_dest = TRI_UpperAsciiString(zone, src);

  if (utf8_dest != nullptr) {
    dstLength = (int32_t)strlen(utf8_dest);
  }
  return utf8_dest;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Extract the words from a UTF-8 string.
////////////////////////////////////////////////////////////////////////////////

bool Utf8Helper::tokenize(std::set<std::string>& words,
                          std::string const& text, size_t minimalLength,
                          size_t maximalLength, bool lowerCase) {
  UErrorCode status = U_ZERO_ERROR;
  UnicodeString word;

  if (text.empty()) {
    return true;
  }
  size_t textLength = text.size();

  if (textLength < minimalLength) {
    // input text is shorter than required minimum length
    return true;
  }

  size_t textUtf16Length = 0;
  UChar* textUtf16 = nullptr;

  if (lowerCase) {
    // lower case string
    int32_t lowerLength = 0;
    char* lower =
        tolower(TRI_UNKNOWN_MEM_ZONE, text.c_str(), (int32_t)textLength, lowerLength);

    if (lower == nullptr) {
      // out of memory
      return false;
    }

    if (lowerLength == 0) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, lower);
      return false;
    }

    textUtf16 = TRI_Utf8ToUChar(TRI_UNKNOWN_MEM_ZONE, lower, lowerLength,
                                &textUtf16Length);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, lower);
  } else {
    textUtf16 = TRI_Utf8ToUChar(TRI_UNKNOWN_MEM_ZONE, text.c_str(), (int32_t)textLength,
                                &textUtf16Length);
  }

  if (textUtf16 == nullptr) {
    return false;
  }

  ULocDataLocaleType type = ULOC_VALID_LOCALE;
  const Locale& locale = _coll->getLocale(type, status);

  if (U_FAILURE(status)) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, textUtf16);
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "error in Collator::getLocale(...): " << u_errorName(status);
    return false;
  }

  UChar* tempUtf16 = (UChar*)TRI_Allocate(
      TRI_UNKNOWN_MEM_ZONE, (textUtf16Length + 1) * sizeof(UChar));

  if (tempUtf16 == nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, textUtf16);
    return false;
  }

  BreakIterator* wordIterator =
      BreakIterator::createWordInstance(locale, status);
  TRI_ASSERT(wordIterator != nullptr);
  UnicodeString utext(textUtf16);

  wordIterator->setText(utext);
  int32_t start = wordIterator->first();
  for (int32_t end = wordIterator->next(); end != BreakIterator::DONE;
       start = end, end = wordIterator->next()) {
    size_t tempUtf16Length = (size_t)(end - start);
    // end - start = word length
    if (tempUtf16Length >= minimalLength) {
      size_t chunkLength = tempUtf16Length;
      if (chunkLength > maximalLength) {
        chunkLength = maximalLength;
      }
      utext.extractBetween(start, (int32_t)(start + chunkLength), tempUtf16, 0);

      size_t utf8WordLength;
      char* utf8Word = TRI_UCharToUtf8(TRI_UNKNOWN_MEM_ZONE, tempUtf16,
                                       chunkLength, &utf8WordLength);
      if (utf8Word != nullptr) {
        std::string word(utf8Word, utf8WordLength);
        words.emplace(word);
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, utf8Word);
      }
    }
  }

  delete wordIterator;

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, textUtf16);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, tempUtf16);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds a regex matcher for the specified pattern
////////////////////////////////////////////////////////////////////////////////

RegexMatcher* Utf8Helper::buildMatcher(std::string const& pattern) {
  UErrorCode status = U_ZERO_ERROR;

  auto matcher = std::make_unique<RegexMatcher>(
      UnicodeString::fromUTF8(pattern), 0, status);
  if (U_FAILURE(status)) {
    return nullptr;
  }

  return matcher.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not value matches a regex
////////////////////////////////////////////////////////////////////////////////

bool Utf8Helper::matches(RegexMatcher* matcher, 
                         char const* value, size_t valueLength, 
                         bool partial, bool& error) {
  TRI_ASSERT(value != nullptr);
  UnicodeString v = UnicodeString::fromUTF8(
      StringPiece(value, static_cast<int32_t>(valueLength)));

  matcher->reset(v);

  UErrorCode status = U_ZERO_ERROR;
  error = false;

  TRI_ASSERT(matcher != nullptr);
  UBool result;

  if (partial) {
    // partial match
    result = matcher->find(status);
  } else {
    // full match
    result = matcher->matches(status);
  }
  if (U_FAILURE(status)) {
    error = true;
  }

  return (result ? true : false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replace value using a regex
////////////////////////////////////////////////////////////////////////////////

std::string Utf8Helper::replace(RegexMatcher* matcher, 
                                char const* value, size_t valueLength, 
                                char const* replacement, size_t replacementLength,
                                bool partial, bool& error) {
  TRI_ASSERT(value != nullptr);
  UnicodeString v = UnicodeString::fromUTF8(
      StringPiece(value, static_cast<int32_t>(valueLength)));
  
  UnicodeString r = UnicodeString::fromUTF8(
      StringPiece(replacement, static_cast<int32_t>(replacementLength)));

  matcher->reset(v);

  UErrorCode status = U_ZERO_ERROR;
  error = false;

  TRI_ASSERT(matcher != nullptr);
  UnicodeString result;

  if (partial) {
    // partial match
    result = matcher->replaceFirst(r, status);
  } else {
    // full match
    result = matcher->replaceAll(r, status);
  }

  if (U_FAILURE(status)) {
    error = true;
    return StaticStrings::Empty;
  }
    
  std::string utf8;
  result.toUTF8String(utf8);
  return utf8;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Lowercase the characters in a UTF-8 string (implemented in
/// Basic/Utf8Helper.cpp)
////////////////////////////////////////////////////////////////////////////////

char* TRI_tolower_utf8(TRI_memory_zone_t* zone, char const* src,
                       int32_t srcLength, int32_t* dstLength) {
  return Utf8Helper::DefaultUtf8Helper.tolower(zone, src, srcLength,
                                               *dstLength);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a utf-8 string to a uchar (utf-16)
////////////////////////////////////////////////////////////////////////////////

UChar* TRI_Utf8ToUChar(TRI_memory_zone_t* zone, char const* utf8,
                       size_t inLength, size_t* outLength) {
  int32_t utf16Length;

  // 1. convert utf8 string to utf16
  // calculate utf16 string length
  UErrorCode status = U_ZERO_ERROR;
  u_strFromUTF8(nullptr, 0, &utf16Length, utf8, (int32_t)inLength, &status);
  if (status != U_BUFFER_OVERFLOW_ERROR) {
    return nullptr;
  }

  UChar* utf16 =
      (UChar*)TRI_Allocate(zone, (utf16Length + 1) * sizeof(UChar));
  if (utf16 == nullptr) {
    return nullptr;
  }

  // now convert
  status = U_ZERO_ERROR;
  // the +1 will append a 0 byte at the end
  u_strFromUTF8(utf16, utf16Length + 1, nullptr, utf8, (int32_t)inLength,
                &status);
  if (status != U_ZERO_ERROR) {
    TRI_Free(zone, utf16);
    return nullptr;
  }

  *outLength = (size_t)utf16Length;

  return utf16;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a uchar (utf-16) to a utf-8 string
////////////////////////////////////////////////////////////////////////////////

char* TRI_UCharToUtf8(TRI_memory_zone_t* zone, UChar const* uchar,
                      size_t inLength, size_t* outLength) {
  int32_t utf8Length;

  // calculate utf8 string length
  UErrorCode status = U_ZERO_ERROR;
  u_strToUTF8(nullptr, 0, &utf8Length, uchar, (int32_t)inLength, &status);

  if (status != U_ZERO_ERROR && status != U_BUFFER_OVERFLOW_ERROR) {
    return nullptr;
  }

  char* utf8 = static_cast<char*>(TRI_Allocate(zone, utf8Length + 1));

  if (utf8 == nullptr) {
    return nullptr;
  }

  // convert to utf8
  status = U_ZERO_ERROR;
  // the +1 will append a 0 byte at the end
  u_strToUTF8(utf8, utf8Length + 1, nullptr, uchar, (int32_t)inLength, &status);

  if (status != U_ZERO_ERROR) {
    TRI_Free(zone, utf8);
    return nullptr;
  }

  *outLength = (size_t)utf8Length;

  return utf8;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize an utf8 string (NFC)
////////////////////////////////////////////////////////////////////////////////

char* TRI_normalize_utf8_to_NFC(TRI_memory_zone_t* zone, char const* utf8,
                                size_t inLength, size_t* outLength) {
  size_t utf16Length;

  *outLength = 0;
  char* utf8Dest;

  if (inLength == 0) {
    utf8Dest = static_cast<char*>(TRI_Allocate(zone, sizeof(char)));

    if (utf8Dest != nullptr) {
      utf8Dest[0] = '\0';
    }
    return utf8Dest;
  }

  UChar* utf16 = TRI_Utf8ToUChar(zone, utf8, inLength, &utf16Length);

  if (utf16 == nullptr) {
    return nullptr;
  }

  // continue in TR_normalize_utf16_to_NFC
  utf8Dest = TRI_normalize_utf16_to_NFC(zone, (uint16_t const*)utf16,
                                        (int32_t)utf16Length, outLength);
  TRI_Free(zone, utf16);

  return utf8Dest;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize an utf8 string (NFC)
////////////////////////////////////////////////////////////////////////////////

char* TRI_normalize_utf16_to_NFC(TRI_memory_zone_t* zone, uint16_t const* utf16,
                                 size_t inLength, size_t* outLength) {
  *outLength = 0;
  char* utf8Dest;

  if (inLength == 0) {
    utf8Dest = static_cast<char*>(TRI_Allocate(zone, sizeof(char)));
    if (utf8Dest != nullptr) {
      utf8Dest[0] = '\0';
    }
    return utf8Dest;
  }

  UErrorCode status = U_ZERO_ERROR;
  UNormalizer2 const* norm2 =
      unorm2_getInstance(nullptr, "nfc", UNORM2_COMPOSE, &status);

  if (status != U_ZERO_ERROR) {
    return nullptr;
  }

  // normalize UChar (UTF-16)
  UChar* utf16Dest;
  bool mustFree;
  char buffer[512];

  if (inLength < sizeof(buffer) / sizeof(UChar)) {
    // use a static buffer
    utf16Dest = (UChar*)&buffer[0];
    mustFree = false;
  } else {
    // use dynamic memory
    utf16Dest =
        (UChar*)TRI_Allocate(zone, (inLength + 1) * sizeof(UChar));
    if (utf16Dest == nullptr) {
      return nullptr;
    }
    mustFree = true;
  }

  size_t overhead = 0;
  int32_t utf16DestLength;

  while (true) {
    status = U_ZERO_ERROR;
    utf16DestLength =
        unorm2_normalize(norm2, (UChar*)utf16, (int32_t)inLength, utf16Dest,
                         (int32_t)(inLength + overhead + 1), &status);

    if (status == U_ZERO_ERROR) {
      break;
    }

    if (status == U_BUFFER_OVERFLOW_ERROR ||
        status == U_STRING_NOT_TERMINATED_WARNING) {
      // output buffer was too small. now re-try with a bigger buffer (inLength
      // + overhead size)
      if (mustFree) {
        // free original buffer first so we don't leak
        TRI_Free(zone, utf16Dest);
        mustFree = false;
      }

      if (overhead == 0) {
        // set initial overhead size
        if (inLength < 256) {
          overhead = 16;
        } else if (inLength < 4096) {
          overhead = 128;
        } else {
          overhead = 256;
        }
      } else {
        // use double buffer size
        overhead += overhead;

        if (overhead >= 1024 * 1024) {
          // enough is enough
          return nullptr;
        }
      }

      utf16Dest = (UChar*)TRI_Allocate(
          zone, (inLength + overhead + 1) * sizeof(UChar));

      if (utf16Dest != nullptr) {
        // got new memory. now try again with the adjusted, bigger buffer
        mustFree = true;
        continue;
      }
      // fall-through intentional
    }

    if (mustFree) {
      TRI_Free(zone, utf16Dest);
    }

    return nullptr;
  }

  // Convert data back from UChar (UTF-16) to UTF-8
  utf8Dest =
      TRI_UCharToUtf8(zone, utf16Dest, (size_t)utf16DestLength, outLength);

  if (mustFree) {
    TRI_Free(zone, utf16Dest);
  }

  return utf8Dest;
}

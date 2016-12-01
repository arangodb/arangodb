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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Rest/Version.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include <openssl/ssl.h>
#include <sstream>

#include <velocypack/Builder.h>
#include <velocypack/Version.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/build-date.h"
#include "Basics/build-repository.h"
#include "Basics/conversions.h"

#include <rocksdb/version.h>

using namespace arangodb::rest;

std::map<std::string, std::string> Version::Values;
  
////////////////////////////////////////////////////////////////////////////////
/// @brief parse a version string into major, minor
/// returns -1, -1 when the version string has an invalid format
/// returns major, -1 when only the major version can be determined
////////////////////////////////////////////////////////////////////////////////
  
std::pair<int, int> Version::parseVersionString(std::string const& str) {
  std::pair<int, int> result{ -1, -1 };

  if (!str.empty()) {
    char const* p = str.c_str();
    char const* q = p;
    while (*q >= '0' && *q <= '9') {
      ++q;
    }
    if (p != q) {
      result.first = std::stoi(std::string(p, q - p));
      result.second = 0;

      if (*q == '.') {
        ++q;
      }
      p = q;
      while (*q >= '0' && *q <= '9') {
        ++q;
      }
      if (p != q) {
        result.second = std::stoi(std::string(p, q - p));
      }
    }
  } 

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

void Version::initialize() {
  if (!Values.empty()) {
    return;
  }

  Values["architecture"] = (sizeof(void*) == 4 ? "32" : "64") + std::string("bit");
  Values["asm-crc32"] = (ENABLE_ASM_CRC32) ? "true" : "false";
  Values["boost-version"] = getBoostVersion();
  Values["build-date"] = getBuildDate();
  Values["compiler"] = getCompiler();
#ifdef _DEBUG
  Values["debug"] = "true";
#else
  Values["debug"] = "false";
#endif
  Values["endianness"] = getEndianness();
  Values["fd-setsize"] = arangodb::basics::StringUtils::itoa(FD_SETSIZE);
  Values["full-version-string"] = getVerboseVersionString();
  Values["icu-version"] = getICUVersion();
  Values["libev-version"] = getLibevVersion();
  Values["openssl-version"] = getOpenSSLVersion();
  Values["platform"] = TRI_PLATFORM;
  Values["server-version"] = getServerVersion();
  Values["sizeof int"] = arangodb::basics::StringUtils::itoa(sizeof(int));
  Values["sizeof void*"] = arangodb::basics::StringUtils::itoa(sizeof(void*));
  Values["v8-version"] = getV8Version();
  Values["vpack-version"] = getVPackVersion();
  Values["zlib-version"] = getZLibVersion();


#if USE_ENTERPRISE
  Values["enterprise-version"] = ARANGODB_ENTERPRISE_VERSION;
#endif

#if HAVE_ARANGODB_BUILD_REPOSITORY
  Values["build-repository"] = getBuildRepository();
#endif

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  Values["assertions"] = "true";
#else
  Values["assertions"] = "false";
#endif

  Values["rocksdb-version"] = std::to_string(ROCKSDB_MAJOR) + "." + std::to_string(ROCKSDB_MINOR) + "." + std::to_string(ROCKSDB_PATCH);

#ifdef __cplusplus
  Values["cplusplus"] = std::to_string(__cplusplus);
#else
  Values["cplusplus"] = "unknown";
#endif

#ifdef __SANITIZE_ADDRESS__
  Values["asan"] = "true";
#else
  Values["asan"] = "false";
#endif

#if defined(__SSE4_2__) && !defined(NO_SSE42)
  Values["sse42"] = "true";
#else
  Values["sse42"] = "false";
#endif

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  Values["maintainer-mode"] = "true";
#else
  Values["maintainer-mode"] = "false";
#endif

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  Values["failure-tests"] = "true";
#else
  Values["failure-tests"] = "false";
#endif

#ifdef ARANGODB_HAVE_TCMALLOC
  Values["tcmalloc"] = "true";
#else
  Values["tcmalloc"] = "false";
#endif

#ifdef ARANGODB_HAVE_JEMALLOC
  Values["jemalloc"] = "true";
#else
  Values["jemalloc"] = "false";
#endif

#ifdef TRI_HAVE_POLL_H
  Values["fd-client-event-handler"] = "poll";
#else
  Values["fd-client-event-handler"] = "select";
#endif
  
  for (auto& it : Values) {
    arangodb::basics::StringUtils::trimInPlace(it.second);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get numeric server version
////////////////////////////////////////////////////////////////////////////////

int32_t Version::getNumericServerVersion() {
  char const* apiVersion = ARANGODB_VERSION;
  char const* p = apiVersion;

  // read major version
  while (*p >= '0' && *p <= '9') {
    ++p;
  }

  TRI_ASSERT(*p == '.');
  int32_t major = TRI_Int32String2(apiVersion, (p - apiVersion));

  apiVersion = ++p;

  // read minor version
  while (*p >= '0' && *p <= '9') {
    ++p;
  }

  TRI_ASSERT((*p == '.' || *p == '-' || *p == '\0') && p != apiVersion);
  int32_t minor = TRI_Int32String2(apiVersion, (p - apiVersion));

  return (int32_t)(minor * 100L + major * 10000L);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get server version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getServerVersion() { return std::string(ARANGODB_VERSION); }

////////////////////////////////////////////////////////////////////////////////
/// @brief get BOOST version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getBoostVersion() {
#ifdef ARANGODB_BOOST_VERSION
  return std::string(ARANGODB_BOOST_VERSION);
#else
  return std::string("");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get V8 version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getV8Version() {
#ifdef ARANGODB_V8_VERSION
  return std::string(ARANGODB_V8_VERSION);
#else
  return std::string("");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get OpenSSL version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getOpenSSLVersion() {
#ifdef OPENSSL_VERSION_TEXT
  return std::string(OPENSSL_VERSION_TEXT);
#elif defined(ARANGODB_OPENSSL_VERSION)
  return std::string(ARANGODB_OPENSSL_VERSION);
#else
  return std::string("");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get libev version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getLibevVersion() {
#ifdef ARANGODB_LIBEV_VERSION
  return std::string(ARANGODB_LIBEV_VERSION);
#else
  return std::string("");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get vpack version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getVPackVersion() {
  return arangodb::velocypack::Version::BuildVersion.toString();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get zlib version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getZLibVersion() {
#ifdef ARANGODB_ZLIB_VERSION
  return std::string(ARANGODB_ZLIB_VERSION);
#else
  return std::string("");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get ICU version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getICUVersion() {
  UVersionInfo icuVersion;
  char icuVersionString[U_MAX_VERSION_STRING_LENGTH];
  u_getVersion(icuVersion);
  u_versionToString(icuVersion, icuVersionString);

  return icuVersionString;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get compiler
////////////////////////////////////////////////////////////////////////////////

std::string Version::getCompiler() {
#if defined(__clang__)
  return "clang [" + std::string(__VERSION__) + "]";
#elif defined(__GNUC__) || defined(__GNUG__)
  return "gcc [" + std::string(__VERSION__) + "]";
#elif defined(_MSC_VER)
  return "msvc [" + std::to_string(_MSC_VER) + "]";
#endif
  return "unknown";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get endianness
////////////////////////////////////////////////////////////////////////////////

std::string Version::getEndianness() {
  uint64_t value = 0x12345678abcdef99;
  static_assert(sizeof(value) == 8, "unexpected uint64_t size");

  unsigned char const* p = reinterpret_cast<unsigned char const*>(&value);
  if (p[0] == 0x12 && p[1] == 0x34 && p[2] == 0x56 && p[3] == 0x78 && p[4] == 0xab && p[5] == 0xcd && p[6] == 0xef && p[7] == 0x99) {
    return "big";
  }
  
  if (p[0] == 0x99 && p[1] == 0xef && p[2] == 0xcd && p[3] == 0xab && p[4] == 0x78 && p[5] == 0x56 && p[6] == 0x34 && p[7] == 0x12) {
    return "little";
  }
  return "unknown";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get build date
////////////////////////////////////////////////////////////////////////////////

std::string Version::getBuildDate() {
// the OpenSuSE build system does not like it, if __DATE__ is used
#ifdef ARANGODB_BUILD_DATE
  return std::string(ARANGODB_BUILD_DATE);
#else
  return std::string(__DATE__).append(" ").append(__TIME__);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get build repository
////////////////////////////////////////////////////////////////////////////////

std::string Version::getBuildRepository() {
#ifdef HAVE_ARANGODB_BUILD_REPOSITORY
  return std::string(ARANGODB_BUILD_REPOSITORY);
#else
  return std::string("");
#endif
}


////////////////////////////////////////////////////////////////////////////////
/// @brief return a server version string
////////////////////////////////////////////////////////////////////////////////

std::string Version::getVerboseVersionString() {
  std::ostringstream version;

  version << "ArangoDB " << ARANGODB_VERSION_FULL << " "
          << (sizeof(void*) == 4 ? "32" : "64") << "bit"
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
          << " maintainer mode"
#endif
#ifdef __SANITIZE_ADDRESS__
          << " with ASAN"
#endif
          << ", using "
#ifdef TRI_HAVE_TCMALLOC
          << "tcmalloc, "
#endif
#ifdef TRI_HAVE_JEMALLOC
          << "jemalloc, "
#endif
          << "VPack " << getVPackVersion() << ", "
          << "ICU " << getICUVersion() << ", "
          << "V8 " << getV8Version() << ", " << getOpenSSLVersion();

  return version.str();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get detailed version information as a (multi-line) string
////////////////////////////////////////////////////////////////////////////////

std::string Version::getDetailed() {
  std::string result;

  for (auto& it : Values) {
    std::string const& value = it.second;

    if (!value.empty()) {
      result.append(it.first);
      result.append(": ");
      result.append(it.second);
#ifdef _WIN32
      result += "\r\n";
#else
      result += "\n";
#endif
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief VelocyPack all data
////////////////////////////////////////////////////////////////////////////////

void Version::getVPack(VPackBuilder& dst) {
  TRI_ASSERT(!dst.isClosed());

  for (auto const& it : Values) {
    std::string const& value = it.second;

    if (!value.empty()) {
      dst.add(it.first, VPackValue(value));
    }
  }
}


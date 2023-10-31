////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include <cstdint>
#include <sstream>
#include <string_view>

#include <openssl/ssl.h>

#include <rocksdb/convenience.h>
#include <rocksdb/version.h>

#include <velocypack/Builder.h>
#include <velocypack/Version.h>

#include "Basics/FeatureFlags.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Basics/asio_ns.h"
#include "Basics/build-date.h"
#include "Basics/build-repository.h"
#include "Basics/conversions.h"
#include "Basics/debugging.h"
#include "BuildId/BuildId.h"

#include "../3rdParty/iresearch/core/utils/version_defines.hpp"

using namespace arangodb;
using namespace arangodb::rest;

std::map<std::string, std::string> Version::Values;

// parse a version string into major, minor
/// returns -1, -1 when the version string has an invalid format
/// returns major, -1 when only the major version can be determined
std::pair<int, int> Version::parseVersionString(std::string const& str) {
  std::pair<int, int> result{-1, -1};

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

/// parse a full version string into major, minor, patch
/// returns -1, -1, -1 when the version string has an invalid format
/// returns major, -1, -1 when only the major version can be determined,
/// returns major, minor, -1 when only the major and minor version can be
/// determined.
FullVersion Version::parseFullVersionString(std::string const& str) {
  FullVersion result{-1, -1, -1};
  int tmp;

  if (!str.empty()) {
    char const* p = str.c_str();
    char const* q = p;
    tmp = 0;
    while (*q >= '0' && *q <= '9') {
      tmp = tmp * 10 + *q++ - '0';
    }
    if (p != q) {
      result.major = tmp;
      tmp = 0;

      if (*q == '.') {
        ++q;
      }
      p = q;
      while (*q >= '0' && *q <= '9') {
        tmp = tmp * 10 + *q++ - '0';
      }
      if (p != q) {
        result.minor = tmp;
        tmp = 0;
        if (*q == '.') {
          ++q;
        }
        p = q;
        while (*q >= '0' && *q <= '9') {
          tmp = tmp * 10 + *q++ - '0';
        }
        if (p != q) {
          result.patch = tmp;
        }
      }
    }
  }

  return result;
}

// initialize
void Version::initialize() {
  if (!Values.empty()) {
    return;
  }

  Values["architecture"] =
      (sizeof(void*) == 4 ? "32" : "64") + std::string("bit");
#if defined(__arm__) || defined(__arm64__) || defined(__aarch64__)
  Values["arm"] = "true";
#else
  Values["arm"] = "false";
#endif
  Values["boost-version"] = getBoostVersion();
  Values["build-date"] = getBuildDate();
  Values["compiler"] = getCompiler();
#ifdef _DEBUG
  Values["debug"] = "true";
#else
  Values["debug"] = "false";
#endif
#ifdef ARANGODB_USE_IPO
  Values["ipo"] = "true";
#else
  Values["ipo"] = "false";
#endif
#ifdef NDEBUG
  Values["ndebug"] = "true";
#else
  Values["ndebug"] = "false";
#endif
#ifdef USE_COVERAGE
  Values["coverage"] = "true";
#else
  Values["coverage"] = "false";
#endif
#ifdef ARCHITECTURE_OPTIMIZATIONS
  Values["optimization-flags"] = std::string(ARCHITECTURE_OPTIMIZATIONS);
#endif
  Values["endianness"] = getEndianness();
  Values["fd-setsize"] = basics::StringUtils::itoa(FD_SETSIZE);
  Values["full-version-string"] = getVerboseVersionString();
  Values["icu-version"] = getICUVersion();
  Values["openssl-version-compile-time"] = getOpenSSLVersion(true);
  Values["openssl-version-run-time"] = getOpenSSLVersion(false);
#ifdef __pic__
  Values["pic"] = std::to_string(__pic__);
#else
  Values["pic"] = "none";
#endif
#ifdef __pie__
  Values["pie"] = std::to_string(__pie__);
#else
  Values["pie"] = "none";
#endif
  Values["platform"] = getPlatform();
  Values["reactor-type"] = getBoostReactorType();
  Values["server-version"] = getServerVersion();
  Values["sizeof int"] = basics::StringUtils::itoa(sizeof(int));
  Values["sizeof long"] = basics::StringUtils::itoa(sizeof(long));
  Values["sizeof void*"] = basics::StringUtils::itoa(sizeof(void*));
  // always hard-coded to "false" since 3.12
  Values["unaligned-access"] = "false";
  Values["v8-version"] = getV8Version();
  Values["vpack-version"] = getVPackVersion();
  Values["zlib-version"] = getZLibVersion();

#if USE_ENTERPRISE
  Values["enterprise-version"] = ARANGODB_ENTERPRISE_VERSION;
  Values["license"] = "enterprise";
#else
  Values["license"] = "community";
#endif

#if HAVE_ARANGODB_BUILD_REPOSITORY
  Values["build-repository"] = getBuildRepository();
#endif

#if HAVE_ENTERPRISE_BUILD_REPOSITORY
  Values["enterprise-build-repository"] = getEnterpriseBuildRepository();
#endif

#if HAVE_OSKAR_BUILD_REPOSITORY
  Values["oskar-build-repository"] = getOskarBuildRepository();
#endif

  Values["curl-version"] = "none";

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  Values["assertions"] = "true";
#else
  Values["assertions"] = "false";
#endif

  Values["rocksdb-version"] = getRocksDBVersion();

#ifdef __cplusplus
  Values["cplusplus"] = std::to_string(__cplusplus);
#else
  Values["cplusplus"] = "unknown";
#endif

  Values["asan"] = "false";
#if defined(__SANITIZE_ADDRESS__)
  Values["asan"] = "true";
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
  Values["asan"] = "true";
#endif
#endif

  Values["tsan"] = "false";
#if defined(__SANITIZE_THREAD__)
  Values["tsan"] = "true";
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
  Values["tsan"] = "true";
#endif
#endif

#if defined(__SSE4_2__) && !defined(NO_SSE42)
  Values["sse42"] = "true";
#else
  Values["sse42"] = "false";
#endif

#ifdef __AVX__
  Values["avx"] = "true";
#else
  Values["avx"] = "false";
#endif

#ifdef __AVX2__
  Values["avx2"] = "true";
#else
  Values["avx2"] = "false";
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

#ifdef ARANGODB_HAVE_JEMALLOC
  Values["jemalloc"] = "true";
#else
  Values["jemalloc"] = "false";
#endif

#ifdef USE_MEMORY_PROFILE
  Values["memory-profiler"] = "true";
#else
  Values["memory-profiler"] = "false";
#endif

#ifdef TRI_HAVE_POLL_H
  Values["fd-client-event-handler"] = "poll";
#else
  Values["fd-client-event-handler"] = "select";
#endif

  Values["iresearch-version"] = getIResearchVersion();

#ifdef ARANGODB_HAVE_LIBUNWIND
  Values["libunwind"] = "true";
#else
  Values["libunwind"] = "false";
#endif

  if constexpr (arangodb::build_id::supportsBuildIdReader()) {
    Values["build-id"] =
        basics::StringUtils::encodeHex(arangodb::build_id::getBuildId());
  }

  if (replication2::EnableReplication2) {
    Values["replication2-enabled"] = "true";
  } else {
    Values["replication2-enabled"] = "false";
  }

  for (auto& it : Values) {
    basics::StringUtils::trimInPlace(it.second);
  }
}

// get numeric server version
int32_t Version::getNumericServerVersion() {
  char const* apiVersion = ARANGODB_VERSION;
  char const* p = apiVersion;

  // read major version
  while (*p >= '0' && *p <= '9') {
    ++p;
  }

  TRI_ASSERT(*p == '.');
  int32_t major = NumberUtils::atoi_positive_unchecked<int32_t>(apiVersion, p);

  apiVersion = ++p;

  // read minor version
  while (*p >= '0' && *p <= '9') {
    ++p;
  }

  TRI_ASSERT((*p == '.' || *p == '-' || *p == '\0') && p != apiVersion);
  int32_t minor = NumberUtils::atoi_positive_unchecked<int32_t>(apiVersion, p);

  int32_t patch = 0;
  if (*p == '.') {
    apiVersion = ++p;

    // read minor version
    while (*p >= '0' && *p <= '9') {
      ++p;
    }

    if (p != apiVersion) {
      patch = NumberUtils::atoi_positive_unchecked<int32_t>(apiVersion, p);
    }
  }

  return (int32_t)(patch + minor * 100L + major * 10000L);
}

// get server version
std::string Version::getServerVersion() {
  return std::string(ARANGODB_VERSION);
}

// get BOOST version
std::string Version::getBoostVersion() {
#ifdef ARANGODB_BOOST_VERSION
  return std::string(ARANGODB_BOOST_VERSION);
#else
  return std::string();
#endif
}

// get boost reactor type
std::string Version::getBoostReactorType() {
#if defined(BOOST_ASIO_HAS_IOCP)
  return std::string("iocp");
#elif defined(BOOST_ASIO_HAS_EPOLL)
  return std::string("epoll");
#elif defined(BOOST_ASIO_HAS_KQUEUE)
  return std::string("kqueue");
#elif defined(BOOST_ASIO_HAS_DEV_POLL)
  return std::string("/dev/poll");
#else
  return std::string("select");
#endif
}

// get RocksDB version
std::string Version::getRocksDBVersion() {
  return std::to_string(ROCKSDB_MAJOR) + "." + std::to_string(ROCKSDB_MINOR) +
         "." + std::to_string(ROCKSDB_PATCH);
}

// get V8 version
std::string Version::getV8Version() {
#ifdef USE_V8
#ifdef ARANGODB_V8_VERSION
  return std::string(ARANGODB_V8_VERSION);
#else
  return std::string();
#endif
#else
  return "none";
#endif
}

// get OpenSSL version
std::string Version::getOpenSSLVersion(bool compileTime) {
  if (compileTime) {
#ifdef OPENSSL_VERSION_TEXT
    return std::string(OPENSSL_VERSION_TEXT);
#elif defined(ARANGODB_OPENSSL_VERSION)
    return std::string(ARANGODB_OPENSSL_VERSION);
#else
    return std::string("openssl (unknown version)");
#endif
  } else {
    char const* v = SSLeay_version(SSLEAY_VERSION);

    if (v == nullptr) {
      return std::string("openssl (unknown version)");
    }

    return std::string(v);
  }
}

// get vpack version
std::string Version::getVPackVersion() {
  return velocypack::Version::BuildVersion.toString();
}

// get zlib version
std::string Version::getZLibVersion() {
#ifdef ARANGODB_ZLIB_VERSION
  return std::string(ARANGODB_ZLIB_VERSION);
#else
  return std::string();
#endif
}

// get ICU version
std::string Version::getICUVersion() {
  UVersionInfo icuVersion;
  char icuVersionString[U_MAX_VERSION_STRING_LENGTH];
  u_getVersion(icuVersion);
  u_versionToString(icuVersion, icuVersionString);

  return icuVersionString;
}

// get IResearch version
std::string Version::getIResearchVersion() { return IResearch_version; }

// get compiler
std::string Version::getCompiler() {
#if defined(__clang__)
  return "clang [" + std::string(__VERSION__) + "]";
#elif defined(__GNUC__) || defined(__GNUG__)
  return "gcc [" + std::string(__VERSION__) + "]";
#elif defined(_MSC_VER)
  return "msvc [" + std::to_string(_MSC_VER) + "]";
#else
  return "unknown";
#endif
}

// get endianness
std::string Version::getEndianness() {
  uint64_t value = 0x12345678abcdef99;
  static_assert(sizeof(value) == 8, "unexpected uint64_t size");

  unsigned char const* p = reinterpret_cast<unsigned char const*>(&value);

  // cppcheck-suppress objectIndex
  if (p[0] == 0x12 && p[1] == 0x34 && p[2] == 0x56 && p[3] == 0x78 &&
      // cppcheck-suppress objectIndex
      p[4] == 0xab && p[5] == 0xcd && p[6] == 0xef && p[7] == 0x99) {
    return "big";
  }

  // cppcheck-suppress objectIndex
  if (p[0] == 0x99 && p[1] == 0xef && p[2] == 0xcd && p[3] == 0xab &&
      // cppcheck-suppress objectIndex
      p[4] == 0x78 && p[5] == 0x56 && p[6] == 0x34 && p[7] == 0x12) {
    return "little";
  }
  return "unknown";
}

std::string Version::getPlatform() { return TRI_PLATFORM; }

// get build date
std::string Version::getBuildDate() {
// the OpenSuSE build system does not like it, if __DATE__ is used
#ifdef ARANGODB_BUILD_DATE
  return std::string(ARANGODB_BUILD_DATE);
#else
  return std::string(__DATE__).append(" ").append(__TIME__);
#endif
}

// get build repository
std::string Version::getBuildRepository() {
#ifdef HAVE_ARANGODB_BUILD_REPOSITORY
  return std::string(ARANGODB_BUILD_REPOSITORY);
#else
  return std::string();
#endif
}

std::string Version::getEnterpriseBuildRepository() {
#ifdef HAVE_ENTERPRISE_BUILD_REPOSITORY
  return std::string(ENTERPRISE_BUILD_REPOSITORY);
#else
  return std::string();
#endif
}

std::string Version::getOskarBuildRepository() {
#ifdef HAVE_OSKAR_BUILD_REPOSITORY
  return std::string(OSKAR_BUILD_REPOSITORY);
#else
  return std::string();
#endif
}

std::string const& Version::getBuildId() {
  auto it = Values.find("build-id");
  if (it == Values.end()) {
    return StaticStrings::Empty;
  }
  return (*it).second;
}

// return a server version string
std::string Version::getVerboseVersionString() {
  std::ostringstream version;

  version << "ArangoDB " << ARANGODB_VERSION_FULL << " "
          << (sizeof(void*) == 4 ? "32" : "64") << "bit";
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  version << " maintainer mode";
#endif

#if defined(__SANITIZE_ADDRESS__)
  version << " with ASAN";
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
  version << " with ASAN";
#endif
#endif

  version << ", using ";
#ifdef ARANGODB_HAVE_JEMALLOC
  version << "jemalloc, ";
#endif

#ifdef HAVE_ARANGODB_BUILD_REPOSITORY
  version << "build " << getBuildRepository() << ", ";
#endif
  version << "VPack " << getVPackVersion() << ", "
          << "RocksDB " << getRocksDBVersion() << ", "
          << "ICU " << getICUVersion() << ", "
#ifdef USE_V8
          << "V8 " << getV8Version() << ", "
#endif
          << getOpenSSLVersion(false);

  if (Values.contains("build-id")) {
    version << ", build-id: " << Values["build-id"];
  }

  return version.str();
}

// get detailed version information as a (multi-line) string
std::string Version::getDetailed() {
  std::string result;

  for (auto const& it : Values) {
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

// VelocyPack all data
void Version::getVPack(VPackBuilder& dst) {
  TRI_ASSERT(!dst.isClosed());

  for (auto const& it : Values) {
    std::string const& value = it.second;

    if (!value.empty()) {
      dst.add(it.first, VPackValue(value));
    }
  }
}

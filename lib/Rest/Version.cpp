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

#include "Basics/conversions.h"
#include "Basics/json.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"

#include <openssl/ssl.h>
#include <sstream>

#include <velocypack/Builder.h>
#include <velocypack/Version.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize
////////////////////////////////////////////////////////////////////////////////

void Version::initialize() {
  if (!Values.empty()) {
    return;
  }

  Values["architecture"] =
      (sizeof(void*) == 4 ? "32" : "64") + std::string("bit");
  Values["server-version"] = getServerVersion();
  Values["icu-version"] = getICUVersion();
  Values["openssl-version"] = getOpenSSLVersion();
  Values["v8-version"] = getV8Version();
  Values["libev-version"] = getLibevVersion();
  Values["vpack-version"] = getVPackVersion();
  Values["zlib-version"] = getZLibVersion();
  Values["configure"] = getConfigure();
  Values["env"] = getConfigureEnvironment();
  Values["build-date"] = getBuildDate();
  Values["repository-version"] = getRepositoryVersion();
  Values["sizeof int"] = arangodb::basics::StringUtils::itoa(sizeof(int));
  Values["sizeof void*"] = arangodb::basics::StringUtils::itoa(sizeof(void*));
  Values["fd-setsize"] = arangodb::basics::StringUtils::itoa(FD_SETSIZE);
#ifdef TRI_ENABLE_MAINTAINER_MODE
  Values["maintainer-mode"] = "true";
#else
  Values["maintainer-mode"] = "false";
#endif

#ifdef TRI_HAVE_TCMALLOC
  Values["tcmalloc"] = "true";
#else
  Values["tcmalloc"] = "false";
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
  char const* apiVersion = TRI_VERSION;
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

std::string Version::getServerVersion() { return std::string(TRI_VERSION); }

////////////////////////////////////////////////////////////////////////////////
/// @brief get V8 version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getV8Version() {
#ifdef TRI_V8_VERSION
  return std::string(TRI_V8_VERSION);
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
#else
  return std::string("");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get libev version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getLibevVersion() {
#ifdef TRI_LIBEV_VERSION
  return std::string(TRI_LIBEV_VERSION);
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
#ifdef TRI_ZLIB_VERSION
  return std::string(TRI_ZLIB_VERSION);
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
/// @brief get configure
////////////////////////////////////////////////////////////////////////////////

std::string Version::getConfigure() {
  std::string configure("");

#ifdef TRI_CONFIGURE_COMMAND
#ifdef TRI_CONFIGURE_OPTIONS
  configure.append(TRI_CONFIGURE_COMMAND).append(TRI_CONFIGURE_OPTIONS);
#endif
#endif
  return configure;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get configure environment
////////////////////////////////////////////////////////////////////////////////

std::string Version::getConfigureEnvironment() {
  std::string env("");

#ifdef TRI_CONFIGURE_FLAGS
  env.append(TRI_CONFIGURE_FLAGS);
#endif
  return env;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get repository version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getRepositoryVersion() {
#ifdef TRI_REPOSITORY_VERSION
  return std::string(TRI_REPOSITORY_VERSION);
#else
  return std::string("");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get build date
////////////////////////////////////////////////////////////////////////////////

std::string Version::getBuildDate() {
// the OpenSuSE build system does not like it, if __DATE__ is used
#ifdef TRI_BUILD_DATE
  return std::string(TRI_BUILD_DATE).append(" ").append(__TIME__);
#else
  return std::string(__DATE__).append(" ").append(__TIME__);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a server version string
////////////////////////////////////////////////////////////////////////////////

std::string Version::getVerboseVersionString() {
  std::ostringstream version;

  version << "ArangoDB " << TRI_VERSION_FULL << " "
          << (sizeof(void*) == 4 ? "32" : "64") << "bit"
#ifdef TRI_ENABLE_MAINTAINER_MODE
          << " maintainer mode"
#endif
          << ", using "
#ifdef TRI_HAVE_TCMALLOC
          << "tcmalloc, "
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
/// @brief JSONize all data
////////////////////////////////////////////////////////////////////////////////

void Version::getJson(TRI_memory_zone_t* zone, TRI_json_t* dst) {
  for (auto const& it : Values) {
    std::string const& value = it.second;

    if (!value.empty()) {
      std::string const& key = it.first;

      TRI_Insert3ObjectJson(
          zone, dst, key.c_str(),
          TRI_CreateStringCopyJson(zone, value.c_str(), value.size()));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief VelocyPack all data
////////////////////////////////////////////////////////////////////////////////

void Version::getVPack(VPackBuilder& dst) {
  for (auto const& it : Values) {
    std::string const& value = it.second;

    if (!value.empty()) {
      dst.add(it.first, VPackValue(value));
    }
  }
}

std::map<std::string, std::string> Version::Values;

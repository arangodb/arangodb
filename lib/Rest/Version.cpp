////////////////////////////////////////////////////////////////////////////////
/// @brief server version information
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
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

using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                     class Version
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                           public static functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise
////////////////////////////////////////////////////////////////////////////////

void Version::initialise () {
  if (! Values.empty()) {
    return;
  }

  Values["architecture"] = (sizeof(void*) == 4 ? "32" : "64") + std::string("bit");
  Values["server-version"] = getServerVersion();
  Values["icu-version"] = getICUVersion();
  Values["openssl-version"] = getOpenSSLVersion();
  Values["v8-version"] = getV8Version();
  Values["libev-version"] = getLibevVersion();
  Values["zlib-version"] = getZLibVersion();
  Values["configure"] = getConfigure();
  Values["env"] = getConfigureEnvironment();
  Values["build-date"] = getBuildDate();
  Values["repository-version"] = getRepositoryVersion();
  Values["sizeof int"] = triagens::basics::StringUtils::itoa(sizeof(int));
  Values["sizeof void*"] = triagens::basics::StringUtils::itoa(sizeof(void*));
#ifdef TRI_ENABLE_MAINTAINER_MODE
  Values["maintainer-mode"] = "true";
#else
  Values["maintainer-mode"] = "false";
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get numeric server version
////////////////////////////////////////////////////////////////////////////////

int32_t Version::getNumericServerVersion () {
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

  return (int32_t) (minor * 100L + major * 10000L);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get server version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getServerVersion () {
  return std::string(TRI_VERSION);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get V8 version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getV8Version () {
#ifdef TRI_V8_VERSION
  return std::string(TRI_V8_VERSION);
#else
  return std::string("");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get OpenSSL version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getOpenSSLVersion () {
#ifdef OPENSSL_VERSION_TEXT
  return std::string(OPENSSL_VERSION_TEXT);
#else
  return std::string("");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get libev version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getLibevVersion () {
#ifdef TRI_LIBEV_VERSION
  return std::string(TRI_LIBEV_VERSION);
#else
  return std::string("");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get zlib version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getZLibVersion () {
#ifdef TRI_ZLIB_VERSION
  return std::string(TRI_ZLIB_VERSION);
#else
  return std::string("");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get ICU version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getICUVersion () {
  UVersionInfo icuVersion;
  char icuVersionString[U_MAX_VERSION_STRING_LENGTH];
  u_getVersion(icuVersion);
  u_versionToString(icuVersion, icuVersionString);

  return icuVersionString;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get configure
////////////////////////////////////////////////////////////////////////////////

std::string Version::getConfigure () {
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

std::string Version::getConfigureEnvironment () {
  std::string env("");

#ifdef TRI_CONFIGURE_FLAGS
  env.append(TRI_CONFIGURE_FLAGS);
#endif
  return env;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get repository version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getRepositoryVersion () {
#ifdef TRI_REPOSITORY_VERSION
  return std::string(TRI_REPOSITORY_VERSION);
#else
  return std::string("");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get build date
////////////////////////////////////////////////////////////////////////////////

std::string Version::getBuildDate () {
  // the OpenSuSE build system does not liked it, if __DATE__ is used
#ifdef TRI_BUILD_DATE
  return std::string(TRI_BUILD_DATE).append(" ").append(__TIME__);
#else
  return std::string(__DATE__).append(" ").append(__TIME__);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a server version string
////////////////////////////////////////////////////////////////////////////////

std::string Version::getVerboseVersionString () {
  std::ostringstream version;

  version << "ArangoDB "
          << getServerVersion()
          << " " << (sizeof(void*) == 4 ? "32" : "64") << "bit"
#ifdef TRI_ENABLE_MAINTAINER_MODE
          << " maintainer mode"
#endif
          << " -- "
          << "ICU " << getICUVersion() << ", "
          << "V8 " << getV8Version() << ", "
          << getOpenSSLVersion();

  return version.str();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get detailed version information as a (multi-line) string
////////////////////////////////////////////////////////////////////////////////

std::string Version::getDetailed () {
  std::string result;

  for (auto& it : Values) {
    std::string value = it.second;
    triagens::basics::StringUtils::trimInPlace(value);

    if (! value.empty()) {
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
/// @brief JSONise all data
////////////////////////////////////////////////////////////////////////////////

void Version::getJson (TRI_memory_zone_t* zone, TRI_json_t* dst) {
  for (auto& it : Values) {
    std::string value = it.second;
    triagens::basics::StringUtils::trimInPlace(value);

    if (! value.empty()) {
      std::string const& key = it.first;

      TRI_Insert3ObjectJson(zone, dst, key.c_str(), TRI_CreateString2CopyJson(zone, value.c_str(), value.size()));
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                           public static variables
// -----------------------------------------------------------------------------

std::map<std::string, std::string> Version::Values;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

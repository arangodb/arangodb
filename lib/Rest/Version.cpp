////////////////////////////////////////////////////////////////////////////////
/// @brief server version information
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Rest/Version.h"

#ifdef _WIN32
#include "BasicsC/win-utils.h"
#endif

#include "build.h"
#include "Basics/Common.h"
#include "Basics/Utf8Helper.h"
#include "BasicsC/json.h"
#include "HttpServer/ApplicationEndpointServer.h"

#include <v8.h>
#include <openssl/ssl.h>

using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                     class Version
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                           public static functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise
////////////////////////////////////////////////////////////////////////////////

void Version::initialise () {
  if (Values.size() > 0) {
    return;
  }

  Values["server-version"] = getServerVersion();
  Values["icu-version"] = getICUVersion();
  Values["openssl-version"] = getOpenSSLVersion();
  Values["v8-version"] = getV8Version();
  Values["libev-version"] = getLibevVersion();
  Values["configure"] = getConfigure();
  Values["env"] = getConfigureEnvironment();
  Values["build-date"] = getBuildDate();
  Values["repository-version"] = getRepositoryVersion();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get server version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getServerVersion () {
  return std::string(TRIAGENS_VERSION);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get V8 version
////////////////////////////////////////////////////////////////////////////////

std::string Version::getV8Version () {
  return v8::V8::GetVersion();
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
  return std::string(__DATE__).append(" ").append(__TIME__);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a server version string
////////////////////////////////////////////////////////////////////////////////

std::string Version::getVerboseVersionString () {
  std::ostringstream version;

  version << "ArangoDB " << getServerVersion() <<
    " -- " <<
    "ICU " << getICUVersion() << ", " <<
    "V8 version " << getV8Version() << ", "
    "SSL engine " << getOpenSSLVersion();

  return version.str();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get detailed version information as a (multi-line) string
////////////////////////////////////////////////////////////////////////////////

std::string Version::getDetailed () {
  std::string result;
  std::map<std::string, std::string>::const_iterator it;

  for (it = Values.begin(); it != Values.end(); ++it) {
    std::string value = (*it).second;
    triagens::basics::StringUtils::trimInPlace(value);

    if (value.size() > 0) {
      result.append((*it).first);
      result.append(": ");
      result.append((*it).second);
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
  std::map<std::string, std::string>::const_iterator it;

  for (it = Values.begin(); it != Values.end(); ++it) {
    std::string value = (*it).second;
    triagens::basics::StringUtils::trimInPlace(value);

    if (value.size() > 0) {
      const string& key = (*it).first;

      TRI_Insert3ArrayJson(zone, dst, key.c_str(), TRI_CreateString2CopyJson(zone, value.c_str(), value.size()));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                           public static variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, std::string> Version::Values;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_REST_VERSION_H
#define ARANGODB_REST_VERSION_H 1

#include <map>
#include <string>

#include "Basics/operating-system.h"

#include "Basics/build.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Basics/Version.h"

#ifndef ARANGODB_ENTERPRISE_VERSION
#error "Enterprise Edition version number is not defined"
#endif

#ifndef NDEBUG
// no -DNEBUG... so this will be very slow
#define ARANGODB_VERSION_FULL                                        \
  ARANGODB_VERSION " " ARANGODB_ENTERPRISE_VERSION " [" TRI_PLATFORM \
                   "-NO-NDEBUG]"

#else

#ifdef _DEBUG
#define ARANGODB_VERSION_FULL \
  ARANGODB_VERSION " " ARANGODB_ENTERPRISE_VERSION " [" TRI_PLATFORM "-DEBUG]"
#else
#define ARANGODB_VERSION_FULL \
  ARANGODB_VERSION " " ARANGODB_ENTERPRISE_VERSION " [" TRI_PLATFORM "]"
#endif

#endif

#else

#ifdef ARANGODB_ENTERPRISE_VERSION
#error "Enterprise Edition version number should not be defined"
#endif

#ifdef _DEBUG
#define ARANGODB_VERSION_FULL ARANGODB_VERSION " [" TRI_PLATFORM "-DEBUG]"
#else
#define ARANGODB_VERSION_FULL ARANGODB_VERSION " [" TRI_PLATFORM "]"
#endif

#endif

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace rest {

class Version {
 private:
  // create the version information
  Version() = delete;
  Version(Version const&) = delete;
  Version& operator=(Version const&) = delete;

 public:
  // parse a version string into major, minor
  /// returns -1, -1 when the version string has an invalid format
  static std::pair<int, int> parseVersionString(std::string const&);

  // initialize
  static void initialize();

  // get numeric server version
  static int32_t getNumericServerVersion();

  // get server version
  static std::string getServerVersion();

  // get BOOST version
  static std::string getBoostVersion();

  // get boost reactor type
  static std::string getBoostReactorType();

  // get RocksDB version
  static std::string getRocksDBVersion();

  // get V8 version
  static std::string getV8Version();

  // get OpenSSL version
  static std::string getOpenSSLVersion(bool compileTime);

  // get vpack version
  static std::string getVPackVersion();

  // get zlib version
  static std::string getZLibVersion();

  // get ICU version
  static std::string getICUVersion();

  // get IResearch version
  static std::string getIResearchVersion();

  // get compiler
  static std::string getCompiler();

  // get endianness
  static std::string getEndianness();

  // get build date
  static std::string getBuildDate();

  // get build repository
  static std::string getBuildRepository();
  static std::string getEnterpriseBuildRepository();
  static std::string getOskarBuildRepository();

  // return a server version string
  static std::string getVerboseVersionString();

  // get detailed version information as a (multi-line) string
  static std::string getDetailed();

  // VelocyPack all data
  static void getVPack(arangodb::velocypack::Builder&);

 public:
  static std::map<std::string, std::string> Values;
};
}  // namespace rest
}  // namespace arangodb

#endif

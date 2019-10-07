////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Baesler
////////////////////////////////////////////////////////////////////////////////

#include "Auth/Common.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"

#include <velocypack/StringRef.h>

using namespace arangodb;

static_assert(auth::Level::UNDEFINED < auth::Level::NONE, "undefined < none");
static_assert(auth::Level::NONE < auth::Level::RO, "none < ro");
static_assert(auth::Level::RO < auth::Level::RW, "none < ro");

static auth::Level _convertToAuthLevel(arangodb::velocypack::StringRef ref) {
  if (ref.compare("rw") == 0) {
    return auth::Level::RW;
  } else if (ref.compare("ro") == 0) {
    return auth::Level::RO;
  } else if (ref.compare("none") == 0 || ref.empty()) {
    return auth::Level::NONE;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                 "expecting access type 'rw', 'ro' or 'none'");
}

auth::Level arangodb::auth::convertToAuthLevel(velocypack::Slice grants) {
  return _convertToAuthLevel(arangodb::velocypack::StringRef(grants));
}

auth::Level arangodb::auth::convertToAuthLevel(std::string const& grants) {
  return _convertToAuthLevel(arangodb::velocypack::StringRef(grants));
}

std::string arangodb::auth::convertFromAuthLevel(auth::Level lvl) {
  if (lvl == auth::Level::RW) {
    return "rw";
  } else if (lvl == auth::Level::RO) {
    return "ro";
  } else if (lvl == auth::Level::NONE) {
    return "none";
  } else {
    return "undefined";
  }
}

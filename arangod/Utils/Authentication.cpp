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

#include "Authentication.h"
#include "Basics/Exceptions.h"
#include "Basics/StringRef.h"
#include "Logger/Logger.h"

using namespace arangodb;

static AuthLevel _convertToAuthLevel(arangodb::StringRef ref) {
  if (ref.compare("rw") == 0) {
    return AuthLevel::RW;
  } else if (ref.compare("ro") == 0) {
    return AuthLevel::RO;
  } else if (ref.compare("none") == 0 || ref.empty()) {
    return AuthLevel::NONE;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                 "expecting access type 'rw', 'ro' or 'none'");
}

AuthLevel arangodb::convertToAuthLevel(velocypack::Slice grants) {
  return _convertToAuthLevel(StringRef(grants));
}

AuthLevel arangodb::convertToAuthLevel(std::string const& grants) {
  return _convertToAuthLevel(StringRef(grants));
}

std::string arangodb::convertFromAuthLevel(AuthLevel lvl) {
  if (lvl == AuthLevel::RW) {
    return "rw";
  } else if (lvl == AuthLevel::RO) {
    return "ro";
  } else {
    return "none";
  }
}

AuthenticationResult DefaultAuthenticationHandler::authenticate(
    std::string const& username, std::string const& password) {
  return AuthenticationResult(TRI_ERROR_USER_NOT_FOUND, AuthSource::COLLECTION);
}

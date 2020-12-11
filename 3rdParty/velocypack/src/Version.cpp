////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include <sstream>

#include "velocypack/velocypack-common.h"
#include "velocypack/Version.h"

using namespace arangodb::velocypack;

Version const Version::BuildVersion(VELOCYPACK_VERSION_MAJOR,
                                    VELOCYPACK_VERSION_MINOR,
                                    VELOCYPACK_VERSION_PATCH);

std::string Version::toString() const {
  std::stringstream version;
  version << majorValue << "." << minorValue << "." << patchValue;
  return version.str();
}

int Version::compare(Version const& other) const {
  if (majorValue != other.majorValue) {
    return majorValue < other.majorValue ? -1 : 1;
  }
  if (minorValue != other.minorValue) {
    return minorValue < other.minorValue ? -1 : 1;
  }
  if (patchValue != other.patchValue) {
    return patchValue < other.patchValue ? -1 : 1;
  }
  return 0;
}

int Version::compare(Version const& lhs, Version const& rhs) {
  return lhs.compare(rhs);
}

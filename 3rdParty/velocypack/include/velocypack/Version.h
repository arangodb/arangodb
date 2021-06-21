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

#ifndef VELOCYPACK_VERSION_H
#define VELOCYPACK_VERSION_H 1

#include <string>

#include "velocypack/velocypack-common.h"
#include "velocypack/velocypack-version-number.h"

namespace arangodb {
namespace velocypack {

struct Version {
  Version() = delete;
  Version(Version const&) = delete;
  Version& operator=(Version const&) = delete;

  Version(int majorValue, int minorValue, int patchValue)
      : majorValue(majorValue),
        minorValue(minorValue),
        patchValue(patchValue) {}

  std::string toString() const;

  int compare(Version const& other) const;

  static int compare(Version const& lhs, Version const& rhs);

  int const majorValue;
  int const minorValue;
  int const patchValue;

  static Version const BuildVersion;
};

}  // namespace arangodb::velocypack
}  // namespace arangodb

#endif

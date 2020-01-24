////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_COMPARE_H
#define VELOCYPACK_COMPARE_H 1

#include "velocypack/velocypack-common.h"

namespace arangodb {
namespace velocypack {
struct Options;
class Slice;

// helper struct for comparing VelocyPack Slices on a binary level
struct BinaryCompare {
// returns true if the two Slices are identical on the binary level
static bool equals(Slice lhs, Slice rhs);

struct Hash {
  size_t operator()(arangodb::velocypack::Slice const&) const;
};
  
struct Equal {
  arangodb::velocypack::Options const* _options;

  Equal() : _options(nullptr) {}
  explicit Equal(arangodb::velocypack::Options const* opts)
      : _options(opts) {}

  bool operator()(arangodb::velocypack::Slice const&,
                  arangodb::velocypack::Slice const&) const;
};

};

// helper struct for comparing VelocyPack Slices in a normalized way
struct NormalizedCompare {

// function to compare two numeric values
static bool equalsNumbers(Slice lhs, Slice rhs);

// function to compare two string values
static bool equalsStrings(Slice lhs, Slice rhs);

// function to compare two arbitrary Slices
static bool equals(Slice lhs, Slice rhs);

struct Hash {
  size_t operator()(arangodb::velocypack::Slice const&) const;
};
  
struct Equal {
  arangodb::velocypack::Options const* _options;

  Equal() : _options(nullptr) {}
  explicit Equal(arangodb::velocypack::Options const* opts)
      : _options(opts) {}

  bool operator()(arangodb::velocypack::Slice const&,
                  arangodb::velocypack::Slice const&) const;
};

};
  
}
}

#endif

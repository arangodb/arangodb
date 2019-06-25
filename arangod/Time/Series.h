////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_TIME_SERIES_H
#define ARANGOD_TIME_SERIES_H 1

#include <cstdint>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
namespace time {
  
struct LabelInfo {
  explicit LabelInfo(velocypack::Slice);
  
  void toVelocyPack(arangodb::velocypack::Builder&) const;
  
public:
  std::string name;
  uint16_t numBuckets;
};

struct Series {
  explicit Series(velocypack::Slice);
  
  void toVelocyPack(arangodb::velocypack::Builder&) const;
  
  /// calculate 2-byte bucket ID
  uint16_t bucketId(arangodb::velocypack::Slice) const;
  
 public:
  std::vector<LabelInfo> labels;
};
  
} // namespace time
} // namespace arangodb

#endif

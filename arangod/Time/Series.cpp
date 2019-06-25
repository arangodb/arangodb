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

#include "Series.h"

#include "Basics/Common.h"
#include "Basics/Exceptions.h"

#include <cmath>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::time;

LabelInfo::LabelInfo(VPackSlice info)
  : name(info.get("name").copyString()),
    numBuckets(info.get("buckets").getNumber<uint16_t>()) {}

void LabelInfo::toVelocyPack(VPackBuilder& b) const {
  b.openObject(/*unindexed*/true);
  b.add("name", VPackValue(name));
  b.add("buckets", VPackValue(numBuckets));
  b.close();
}

Series::Series(VPackSlice info)
: labels() {
  
  VPackSlice bs = info.get("labels");
  if (bs.isArray()) {
    return;
  }
  uint64_t prod = 1;
  for (VPackSlice slice : VPackArrayIterator(bs)) {
    labels.emplace_back(slice);
    if (labels.back().numBuckets == 0) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "wrong bucket size");
    }
    prod *= labels.back().numBuckets;
    if (prod >= std::numeric_limits<uint16_t>::max()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "too many buckets");
    }
  }
}

void Series::toVelocyPack(VPackBuilder& b) const {
  b.add(VPackValue("labels"));
  b.openArray(/*unindexed*/true);
  for (LabelInfo const& i : labels) {
    i.toVelocyPack(b);
  }
  b.close();
}


/// calculate 2-byte bucket ID
uint16_t Series::bucketId(arangodb::velocypack::Slice slice) const {
  TRI_ASSERT(slice.isObject());

  uint64_t bucketId = 0;
  uint64_t prodBuckets = 1;
  for (LabelInfo const& label : labels) {
    VPackSlice key = slice.get(label.name);
    uint64_t hash = 0;
    if (!key.isNone() && !key.isNull()) {
      hash = key.normalizedHash();
      hash = hash % label.numBuckets;
    }
    bucketId += hash * prodBuckets;
    prodBuckets *= label.numBuckets;
  }
  TRI_ASSERT(bucketId <= std::numeric_limits<uint16_t>::max());
  
  return bucketId;
}

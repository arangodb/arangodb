////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Koushal Kawade
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <iostream>
#include <vector>
#include <string>
#include "Activities/RegistryGlobalVariable.h"
#include "Activities/GuardedActivity.h"

namespace arangodb::iresearch {

//  SegmentConsolidationActivityEntry
//  Represents info about a single segment which
//  is part of the consolidation candidate
struct SegmentConsolidationActivityEntry {
  std::string name;
  uint64_t docs_count{};
  uint64_t live_docs_count{};
  uint64_t byte_size{};
};

template<typename Inspector>
inline auto inspect(Inspector& f, SegmentConsolidationActivityEntry& x) {
  return f.object(x).fields(f.field("name", x.name),
                            f.field("byteSize", x.byte_size),
                            f.field("docsCount", x.docs_count),
                            f.field("liveDocsCount", x.live_docs_count));
}

//  SegmentConsolidationActivityData
//  Represents info about all the segments comprising
//  a consolidation operation
struct SegmentConsolidationActivityData {
  std::vector<SegmentConsolidationActivityEntry> candidates;
  bool operator==(SegmentConsolidationActivityData const&) const = default;
};

template<typename Inspector>
inline auto inspect(Inspector& f, SegmentConsolidationActivityData& x) {
  return f.object(x).fields(f.field("segments", x.candidates));
}

//  SegmentConsolidationActivity
struct SegmentConsolidationActivity
    : arangodb::activities::GuardedActivity<SegmentConsolidationActivity,
                                            SegmentConsolidationActivityData> {
  SegmentConsolidationActivity(arangodb::activities::ActivityId id,
                               arangodb::activities::ActivityHandle parent,
                               SegmentConsolidationActivityData data)
      : arangodb::activities::GuardedActivity<SegmentConsolidationActivity,
                                              SegmentConsolidationActivityData>(
            id, parent, "ArangoSearchConsolidation", std::move(data)) {}
};

}  // namespace arangodb::iresearch

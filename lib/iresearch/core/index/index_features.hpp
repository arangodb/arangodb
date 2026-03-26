////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <map>
#include <set>
#include <span>

#include "index/column_info.hpp"
#include "utils/bit_utils.hpp"
#include "utils/type_info.hpp"

namespace irs {

struct field_stats;
struct column_output;

// Represents a set of features that can be stored in the index
enum class IndexFeatures : uint8_t {
  // Documents
  NONE = 0,

  // Frequency
  FREQ = 1,

  // Positions, depends on frequency
  POS = 2,

  // Offsets, depends on positions
  OFFS = 4,

  // Payload, depends on positions
  PAY = 8,

  // All features
  ALL = FREQ | POS | OFFS | PAY
};

ENABLE_BITMASK_ENUM(IndexFeatures);

// Return true if 'lhs' is a subset of 'rhs'
IRS_FORCE_INLINE bool IsSubsetOf(IndexFeatures lhs,
                                 IndexFeatures rhs) noexcept {
  return lhs == (lhs & rhs);
}

struct FeatureWriter : memory::Managed {
  using ptr = memory::managed_ptr<FeatureWriter>;

  virtual void write(const field_stats& stats, doc_id_t doc,
                     column_output& writer) = 0;

  virtual void write(data_output& out, bytes_view value) = 0;

  virtual void finish(bstring& out) = 0;
};

using FeatureWriterFactory =
  FeatureWriter::ptr (*)(std::span<const bytes_view>);

using FeatureInfoProvider =
  std::function<std::pair<ColumnInfo, FeatureWriterFactory>(
    type_info::type_id)>;

using feature_map_t = std::map<type_info::type_id, field_id>;
using feature_set_t = std::set<type_info::type_id>;
using features_t = std::span<const type_info::type_id>;

}  // namespace irs

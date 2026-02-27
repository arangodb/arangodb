////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
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

#include <function2/function2.hpp>
#include <functional>

#include "resource_manager.hpp"
#include "search/scorer.hpp"
#include "utils/bit_utils.hpp"

namespace irs {

struct SegmentMeta;
struct field_reader;
struct column_reader;

using ColumnWarmupCallback =
  std::function<bool(const SegmentMeta& meta, const field_reader& fields,
                     const column_reader& column)>;

// Scorers allowed to be used in conjunction with wanderator.
using ScorersView = std::span<const Scorer* const>;

// We support up to 64 scorers per field
inline constexpr size_t kMaxScorers = bits_required<uint64_t>();

struct WandContext {
  static constexpr auto kDisable = std::numeric_limits<uint8_t>::max();

  bool Enabled() const noexcept { return index != kDisable; }

  // Index of the wand data in the IndexWriter to use for optimization.
  // Optimization is turned off by default.
  uint8_t index{kDisable};
  bool strict{false};
  mutable bool root{true};
};

struct IndexReaderOptions {
  ColumnWarmupCallback warmup_columns;

  ResourceManagementOptions resource_manager{
    ResourceManagementOptions::kDefault};

  // A list of wand scorers.
  ScorersView scorers;

  // Open inverted index
  bool index{true};

  // Open columnstore
  bool columnstore{true};

  // Read document mask
  bool doc_mask{true};
};

}  // namespace irs

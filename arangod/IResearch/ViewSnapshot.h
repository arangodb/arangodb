////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Containers/FlatHashMap.h"
#include "Containers/FlatHashSet.h"
#include "IResearch/IResearchDataStore.h"
#include "VocBase/Identifiers/DataSourceId.h"

#include "index/index_reader.hpp"

#include <vector>
#include <string_view>
#include <cstdint>
#include <cstddef>
#include <memory>

namespace arangodb {
namespace transaction {

class Methods;

}  // namespace transaction
namespace iresearch {

//////////////////////////////////////////////////////////////////////////////
/// @brief a snapshot representation of the view with ability to query for cid
//////////////////////////////////////////////////////////////////////////////
class ViewSnapshot : public irs::index_reader {
 public:
  using Links = containers::FlatHashMap<DataSourceId, LinkLock>;

  /// @return cid of the sub-reader at operator['offset'] or 0 if undefined
  [[nodiscard]] virtual DataSourceId cid(std::size_t offset) const noexcept = 0;
};

using ViewSnapshotPtr = std::shared_ptr<ViewSnapshot const>;

class ViewSnapshotImpl : public ViewSnapshot {
 protected:
  using Segments = std::vector<std::pair<DataSourceId, irs::sub_reader const*>>;

  std::uint64_t _live_docs_count = 0;
  std::uint64_t _docs_count = 0;
  Segments _segments;

 private:
  [[nodiscard]] std::uint64_t live_docs_count() const noexcept final {
    return _live_docs_count;
  }

  [[nodiscard]] std::uint64_t docs_count() const noexcept final {
    return _docs_count;
  }

  [[nodiscard]] irs::sub_reader const& operator[](
      std::size_t i) const noexcept final {
    TRI_ASSERT(i < _segments.size());
    return *(_segments[i].second);
  }

  [[nodiscard]] std::size_t size() const noexcept final {
    return _segments.size();
  }

  [[nodiscard]] DataSourceId cid(std::size_t i) const noexcept final {
    return i < _segments.size() ? _segments[i].first : DataSourceId::none();
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief index reader implementation over multiple irs::index_reader
/// @note it is assumed that ViewState resides in the same
///       TransactionState as the IResearchView ViewState, therefore a separate
///       lock is not required to be held
////////////////////////////////////////////////////////////////////////////////
class ViewSnapshotView final : public ViewSnapshotImpl {
 public:
  /// @brief constructs snapshot from a given snapshot
  ///        according to specified set of collections
  ViewSnapshotView(
      const ViewSnapshot& rhs,
      containers::FlatHashSet<DataSourceId> const& collections) noexcept;
};

/// @enum snapshot getting mode
enum class ViewSnapshotMode {
  /// @brief lookup existing snapshot from a transaction
  Find,

  /// @brief lookup existing snapshot from a transaction
  /// or create if it doesn't exist
  FindOrCreate,

  /// @brief retrieve the latest view snapshot and cache it in a transaction
  SyncAndReplace,
};

////////////////////////////////////////////////////////////////////////////////
/// makeViewSnapshot
///
/// @param trx ...
/// @param mode ...
/// @param links ...
/// @param key ...
/// @param name ...
/// @return ...
////////////////////////////////////////////////////////////////////////////////
ViewSnapshot const* makeViewSnapshot(transaction::Methods& trx,
                                     ViewSnapshotMode mode,
                                     ViewSnapshot::Links&& links,
                                     void const* key,
                                     std::string_view name) noexcept;

}  // namespace iresearch
}  // namespace arangodb

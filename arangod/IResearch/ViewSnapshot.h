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

#include "Containers/FlatHashSet.h"
#include "IResearch/IResearchDataStore.h"
#include "VocBase/Identifiers/DataSourceId.h"

#include "index/index_reader.hpp"
#include "search/boolean_filter.hpp"

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
  using Links = std::vector<LinkLock>;
  using Segments = std::vector<std::pair<DataSourceId, irs::sub_reader const*>>;

  /// @return cid of the sub-reader at operator['offset'] or 0 if undefined
  [[nodiscard]] virtual DataSourceId cid(std::size_t offset) const noexcept = 0;

  bool hasNestedFields() const noexcept { return _hasNestedFields; }

  [[nodiscard]] std::uint64_t live_docs_count() const noexcept final {
    return _live_docs_count;
  }

  [[nodiscard]] std::uint64_t docs_count() const noexcept final {
    return _docs_count;
  }

 protected:
  std::uint64_t _live_docs_count = 0;
  std::uint64_t _docs_count = 0;
  bool _hasNestedFields{false};
};

using ViewSnapshotPtr = std::shared_ptr<ViewSnapshot const>;

////////////////////////////////////////////////////////////////////////////////
/// @brief index reader implementation over multiple irs::index_reader
/// @note it is assumed that ViewState resides in the same
///       TransactionState as the IResearchView ViewState, therefore a separate
///       lock is not required to be held
////////////////////////////////////////////////////////////////////////////////
class ViewSnapshotView final : public ViewSnapshot {
 public:
  /// @brief constructs snapshot from a given snapshot
  ///        according to specified set of collections
  ViewSnapshotView(
      const ViewSnapshot& rhs,
      containers::FlatHashSet<DataSourceId> const& collections) noexcept;

  [[nodiscard]] DataSourceId cid(std::size_t i) const noexcept final {
    TRI_ASSERT(i < _segments.size());
    return _segments[i].first;
  }

  [[nodiscard]] irs::sub_reader const& operator[](
      std::size_t i) const noexcept final {
    TRI_ASSERT(i < _segments.size());
    return *(_segments[i].second);
  }

  [[nodiscard]] std::size_t size() const noexcept final {
    return _segments.size();
  }

 private:
  Segments _segments;
};

////////////////////////////////////////////////////////////////////////////////
/// Get view snapshot from transaction state
///
/// @param trx transaction
/// @param key should be a LogicalView* or IResearchViewNode*
////////////////////////////////////////////////////////////////////////////////
ViewSnapshot* getViewSnapshot(transaction::Methods& trx,
                              void const* key) noexcept;

////////////////////////////////////////////////////////////////////////////////
/// Try to commit every link/index in snapshot and then recompute snapshot data
/// Commit different links is not atomic.
///
/// @param snapshot should be a ViewSnapshotCookie
/// @param name of snapshot view
////////////////////////////////////////////////////////////////////////////////
void syncViewSnapshot(ViewSnapshot& snapshot, std::string_view name);

////////////////////////////////////////////////////////////////////////////////
/// Create new ViewSnapshot
///
/// @pre getViewSnapshot(trx, key) == nullptr
/// @post getViewSnapshot(trx, key) == this return
///
/// @param trx transaction
/// @param key should be a LogicalView* or IResearchViewNode*
/// @param sync need commit link/index before take snapshot,
///             commit different links is not atomic
/// @param name of snapshot view
/// @param links LinkLocks for link/index,
///              if its empty return valid empty snapshot
/// @return new snapshot or nullptr if error
////////////////////////////////////////////////////////////////////////////////
ViewSnapshot* makeViewSnapshot(transaction::Methods& trx, void const* key,
                               bool sync, std::string_view name,
                               ViewSnapshot::Links&& links);

struct FilterCookie : TransactionState::Cookie {
  irs::filter::prepared const* filter{};
};

FilterCookie& ensureFilterCookie(transaction::Methods& trx, void const* key);

}  // namespace iresearch
}  // namespace arangodb

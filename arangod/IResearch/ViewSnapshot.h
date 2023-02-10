////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include "search/proxy_filter.hpp"

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

using ViewSegment =
    std::tuple<DataSourceId, irs::SubReader const*, StorageSnapshot const&>;

//////////////////////////////////////////////////////////////////////////////
/// @brief a snapshot representation of the view with ability to query for cid
//////////////////////////////////////////////////////////////////////////////
class ViewSnapshot : public irs::IndexReader {
 public:
  using Links = std::vector<LinkLock>;
  using Segments = std::vector<ViewSegment>;
  using ImmutablePartCache =
      std::map<aql::AstNode const*, irs::proxy_filter::cache_ptr>;

  /// @return cid of the sub-reader at operator['offset'] or 0 if undefined
  [[nodiscard]] virtual DataSourceId cid(std::size_t offset) const noexcept = 0;

  [[nodiscard]] virtual ImmutablePartCache& immutablePartCache() noexcept = 0;

  [[nodiscard]] virtual StorageSnapshot const& snapshot(
      std::size_t i) const noexcept = 0;

  [[nodiscard]] bool hasNestedFields() const noexcept {
    return _hasNestedFields;
  }

  [[nodiscard]] std::uint64_t live_docs_count() const noexcept final {
    return _live_docs_count;
  }

  [[nodiscard]] std::uint64_t docs_count() const noexcept final {
    return _docs_count;
  }

  [[nodiscard]] virtual ViewSegment const& segment(
      std::size_t i) const noexcept = 0;

 protected:
  std::uint64_t _live_docs_count = 0;
  std::uint64_t _docs_count = 0;
  bool _hasNestedFields{false};
};

using ViewSnapshotPtr = std::shared_ptr<ViewSnapshot const>;

////////////////////////////////////////////////////////////////////////////////
/// @brief index reader implementation over multiple irs::IndexReader
/// @note it is assumed that ViewState resides in the same
///       TransactionState as the IResearchView ViewState, therefore a separate
///       lock is not required to be held
////////////////////////////////////////////////////////////////////////////////
/// FIXME: Currently this class is not used as there is an issue if one view
///        is used several times in the query and waitForSync is enabled.
///        In such case ViewSnapshotCookie is refilled several times. So
///        dangling ViewSnapshotView is possible. We could adress this
///        by doing viewSnapshotSync once per view. So this class is not
///        deleted in hope it would be needed again.
// class ViewSnapshotView final : public ViewSnapshot {
// public:
//  /// @brief constructs snapshot from a given snapshot
//  ///        according to specified set of collections
//  ViewSnapshotView(
//      const ViewSnapshot& rhs,
//      containers::FlatHashSet<DataSourceId> const& collections) noexcept;
//
//  [[nodiscard]] DataSourceId cid(std::size_t i) const noexcept final {
//    TRI_ASSERT(i < _segments.size());
//    return std::get<0>(_segments[i]);
//  }
//
//  [[nodiscard]] irs::SubReader const& operator[](
//      std::size_t i) const noexcept final {
//    TRI_ASSERT(i < _segments.size());
//    return *(std::get<1>(_segments[i]));
//  }
//
//  [[nodiscard]] StorageSnapshot const& snapshot(
//      std::size_t i) const noexcept final {
//    TRI_ASSERT(i < _segments.size());
//    return (std::get<2>(_segments[i]));
//  }
//
//  [[nodiscard]] std::size_t size() const noexcept final {
//    return _segments.size();
//  }
//
//  ViewSegment const& segment(std::size_t i) const noexcept final {
//    TRI_ASSERT(i < _segments.size());
//    return _segments[i];
//  }
//
// private:
//  Segments _segments;
//};

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

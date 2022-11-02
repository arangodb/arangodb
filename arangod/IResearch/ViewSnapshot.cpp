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

#include "IResearch/ViewSnapshot.h"
#include "Basics/DownCast.h"
#include "Basics/Exceptions.h"
#include "Containers/FlatHashMap.h"
#include "Transaction/Methods.h"

#include <absl/strings/str_cat.h>

namespace arangodb::iresearch {
namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief index reader implementation over multiple irs::index_reader
///        the container storing the view state for a given TransactionState
/// @note it is assumed that DBServer ViewState resides in the same
///       TransactionState as the IResearchView ViewState, therefore a separate
///       lock is not required to be held by the DBServer CompoundReader
////////////////////////////////////////////////////////////////////////////////
class ViewSnapshotCookie final : public ViewSnapshot,
                                 public TransactionState::Cookie {
 public:
  ViewSnapshotCookie() noexcept = default;

  explicit ViewSnapshotCookie(Links&& links) noexcept;

  void clear() noexcept;

  void compute(bool sync, std::string_view name);

 private:
  [[nodiscard]] irs::sub_reader const& operator[](
      std::size_t i) const noexcept final {
    TRI_ASSERT(i < _segments.size());
    return *(_segments[i].second);
  }

  [[nodiscard]] std::size_t size() const noexcept final {
    return _segments.size();
  }

  [[nodiscard]] DataSourceId cid(std::size_t i) const noexcept final {
    TRI_ASSERT(i < _segments.size());
    return _segments[i].first;
  }

  // prevent data-store deallocation (lock @ AsyncSelf)
  Links _links;  // should be first
  std::vector<irs::directory_reader> _readers;
  Segments _segments;
};

ViewSnapshotCookie::ViewSnapshotCookie(Links&& links) noexcept
    : _links{std::move(links)} {
  _readers.reserve(_links.size());
}

void ViewSnapshotCookie::clear() noexcept {
  _live_docs_count = 0;
  _docs_count = 0;
  _hasNestedFields = false;
  _segments.clear();
  _readers.clear();
}

void ViewSnapshotCookie::compute(bool sync, std::string_view name) {
  size_t segments = 0;
  for (auto& link : _links) {
    TRI_ASSERT(link);
    if (sync) {
      auto r = IResearchDataStore::commit(link, true);
      if (!r.ok()) {
        LOG_TOPIC("fd776", WARN, TOPIC)
            << "failed to sync while creating snapshot for view '" << name
            << "', previous snapshot will be used instead, error: '"
            << r.errorMessage() << "'";
      }
    }
    _readers.push_back(IResearchDataStore::reader(link));
    segments += _readers.back().size();
  }
  _segments.reserve(segments);
  for (size_t i = 0; i != _links.size(); ++i) {
    auto const cid = _links[i]->collection().id();
    auto const& reader = _readers[i];
    for (auto const& segment : reader) {
      _segments.emplace_back(cid, &segment);
    }
    _live_docs_count += reader.live_docs_count();
    _docs_count += reader.docs_count();
    _hasNestedFields |= _links[i]->hasNestedFields();
  }
}

}  // namespace

ViewSnapshotView::ViewSnapshotView(
    const ViewSnapshot& rhs,  // TODO(MBkkt) Maybe we should move?
    containers::FlatHashSet<DataSourceId> const& collections) noexcept {
  for (std::size_t i = 0, size = rhs.size(); i != size; ++i) {
    auto const cid = rhs.cid(i);
    if (!collections.contains(cid)) {
      continue;
    }
    auto const& segment = rhs[i];
    _docs_count += segment.docs_count();
    _live_docs_count += segment.live_docs_count();
    _segments.emplace_back(cid, &segment);
  }
}

ViewSnapshot* getViewSnapshot(transaction::Methods& trx,
                              void const* key) noexcept {
  TRI_ASSERT(key != nullptr);
  TRI_ASSERT(trx.state());
  auto& state = *(trx.state());
  return basics::downCast<ViewSnapshotCookie>(state.cookie(key));
}

FilterCookie& ensureFilterCookie(transaction::Methods& trx, void const* key) {
  TRI_ASSERT(key != nullptr);
  TRI_ASSERT(trx.state());
  auto& state = *(trx.state());

  if (auto* cookie = state.cookie(key); !cookie) {
    auto filterCookie = std::make_unique<FilterCookie>();
    auto* p = filterCookie.get();
    [[maybe_unused]] auto const old =
        state.cookie(key, std::move(filterCookie));
    TRI_ASSERT(!old);
    return *p;
  } else {
    return *basics::downCast<FilterCookie>(cookie);
  }
}

void syncViewSnapshot(ViewSnapshot& snapshot, std::string_view name) {
  auto& ctx = basics::downCast<ViewSnapshotCookie>(snapshot);
  ctx.clear();
  ctx.compute(true, name);
}

ViewSnapshot* makeViewSnapshot(transaction::Methods& trx, void const* key,
                               bool sync, std::string_view name,
                               ViewSnapshot::Links&& links) {
  TRI_ASSERT(trx.state());
  auto& state = *(trx.state());
  TRI_ASSERT(state.cookie(key) == nullptr);

  for (auto const& link : links) {
    if (!link) {
      LOG_TOPIC("fffff", WARN, TOPIC)
          << "failed to lock a link for view '" << name << "'";
      return nullptr;
    }

    if (link->failQueriesOnOutOfSync() && link->isOutOfSync()) {
      // link is out of sync, we cannot use it for querying
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC,
          absl::StrCat("link ", std::to_string(link->id().id()),
                       " has been marked as failed and needs to be recreated"));
    }
  }

  auto cookie = std::make_unique<ViewSnapshotCookie>(std::move(links));
  auto& ctx = *cookie;
  ctx.compute(sync, name);
  state.cookie(key, std::move(cookie));
  return &ctx;
}

}  // namespace arangodb::iresearch

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
#include "Containers/FlatHashMap.h"
#include "Transaction/Methods.h"

namespace arangodb::iresearch {
namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief index reader implementation over multiple irs::index_reader
///        the container storing the view state for a given TransactionState
/// @note it is assumed that DBServer ViewState resides in the same
///       TransactionState as the IResearchView ViewState, therefore a separate
///       lock is not required to be held by the DBServer CompoundReader
////////////////////////////////////////////////////////////////////////////////
class ViewSnapshotCookie final : public ViewSnapshotImpl,
                                 public TransactionState::Cookie {
 public:
  ViewSnapshotCookie() noexcept = default;

  explicit ViewSnapshotCookie(Links&& links) noexcept;

  void clear() noexcept;

  bool compute(bool sync, std::string_view name);

 private:
  std::vector<irs::directory_reader> _readers;
  // prevent data-store deallocation (lock @ AsyncSelf)
  Links _links;
};

ViewSnapshotCookie::ViewSnapshotCookie(Links&& links) noexcept
    : _links{std::move(links)} {
  _readers.reserve(links.size());
}

void ViewSnapshotCookie::clear() noexcept {
  _live_docs_count = 0;
  _docs_count = 0;
  _segments.clear();
  _readers.clear();
}

bool ViewSnapshotCookie::compute(bool sync, std::string_view name) {
  size_t segments = 0;
  for (auto& link : _links) {
    if (!link) {
      LOG_TOPIC("fffff", WARN, TOPIC)
          << "failed to lock a link for view '" << name;
      return false;
    }
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
  }
  return true;
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

void syncViewSnapshot(ViewSnapshot& snapshot, std::string_view name) {
  auto& ctx = basics::downCast<ViewSnapshotCookie>(snapshot);
  ctx.clear();
  TRI_ASSERT(ctx.compute(true, name));
}

ViewSnapshot* makeViewSnapshot(transaction::Methods& trx, void const* key,
                               bool sync, std::string_view name,
                               ViewSnapshot::Links&& links) noexcept {
  if (links.empty()) {
    // cannot be a const, we can call syncViewSnapshot on it,
    // that should do nothing but in generally we cannot prove it
    static ViewSnapshotCookie empty;
    return &empty;  // TODO(MBkkt) Maybe nullptr?
  }
  TRI_ASSERT(trx.state());
  auto& state = *(trx.state());
  TRI_ASSERT(state.cookie(key) == nullptr);
  auto cookie = std::make_unique<ViewSnapshotCookie>(std::move(links));
  auto& ctx = *cookie;
  try {
    if (ctx.compute(sync, name)) {
      state.cookie(key, std::move(cookie));
      return &ctx;
    }
  } catch (basics::Exception const& e) {
    LOG_TOPIC("29b30", WARN, TOPIC)
        << "caught exception while collecting readers for snapshot of view '"
        << name << "', tid '" << state.id() << "': " << e.code() << " "
        << e.what();
  } catch (std::exception const& e) {
    LOG_TOPIC("ffe73", WARN, TOPIC)
        << "caught exception while collecting readers for snapshot of view '"
        << name << "', tid '" << state.id() << "': " << e.what();
  } catch (...) {
    LOG_TOPIC("c54e8", WARN, TOPIC)
        << "caught exception while collecting readers for snapshot of view '"
        << name << "', tid '" << state.id() << "'";
  }
  return nullptr;
}

}  // namespace arangodb::iresearch

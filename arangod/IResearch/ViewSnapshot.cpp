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
  bool equalCollections(Links const& links);

  void clear() noexcept;

  void add(DataSourceId cid, irs::directory_reader&& reader) noexcept;

  void set(Links&& links) noexcept;

  std::vector<irs::directory_reader> _readers;
  // prevent data-store deallocation (lock @ AsyncSelf)
  Links _links;
};

bool ViewSnapshotCookie::equalCollections(Links const& links) {
  if (_links.size() != links.size()) {
    TRI_ASSERT(_links.empty());
    return false;
  }
  for (auto const& entry : _links) {
    if (!links.contains(entry.first)) {
      // key not found -> maybe sync + create + return new key
      // key     found -> return key
      //             \--> sync + recompute + return key
      TRI_ASSERT(false);
      return false;
    }
  }
  return true;
}

void ViewSnapshotCookie::clear() noexcept {
  _live_docs_count = 0;
  _docs_count = 0;
  _segments.clear();
  _readers.clear();
  _links = {};
}

void ViewSnapshotCookie::add(DataSourceId cid,
                             irs::directory_reader&& reader) noexcept {
  _readers.reserve(_readers.size() + reader.size());
  for (auto const& segment : reader) {
    _segments.emplace_back(cid, &segment);
  }
  _live_docs_count += reader.live_docs_count();
  _docs_count += reader.docs_count();
  _readers.emplace_back(std::move(reader));
}

void ViewSnapshotCookie::set(Links&& links) noexcept {
  _links = std::move(links);
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

ViewSnapshot const* makeViewSnapshot(transaction::Methods& trx,
                                     ViewSnapshotMode mode,
                                     ViewSnapshot::Links&& links,
                                     void const* key,
                                     std::string_view name) noexcept {
  if (links.empty()) {
    static const ViewSnapshotImpl empty;
    return &empty;  // TODO(MBkkt) Maybe nullptr?
  }
  TRI_ASSERT(trx.state());
  auto& state = *(trx.state());
  auto* ctx = basics::downCast<ViewSnapshotCookie>(state.cookie(key));
  // We want to check ==, but not >= because ctx also is reader
  if (mode == ViewSnapshotMode::Find) {
    return ctx && ctx->equalCollections(links) ? ctx : nullptr;
  }
  std::unique_ptr<ViewSnapshotCookie> cookie;
  if (ctx) {
    if (mode == ViewSnapshotMode::FindOrCreate &&
        ctx->equalCollections(links)) {
      return ctx;
    }
    ctx->clear();
  } else {
    cookie = std::make_unique<ViewSnapshotCookie>();
    ctx = cookie.get();
  }
  TRI_ASSERT(ctx);
  try {
    for (auto& entry : links) {
      if (!entry.second) {
        LOG_TOPIC("fffff", WARN, TOPIC)
            << "failed to lock a link for collection '" << entry.first
            << "' for view '" << name;
        return nullptr;
      }
      if (mode == ViewSnapshotMode::SyncAndReplace) {
        auto r = IResearchDataStore::commit(entry.second, true);
        if (!r.ok()) {
          LOG_TOPIC("fd776", WARN, TOPIC)
              << "failed to sync while creating snapshot for view '" << name
              << "', previous snapshot will be used instead, error: '"
              << r.errorMessage() << "'";
        }
      }
      ctx->add(entry.first, IResearchDataStore::reader(entry.second));
    }
    ctx->set(std::move(links));
    if (cookie) {
      state.cookie(key, std::move(cookie));
    }
    return ctx;
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

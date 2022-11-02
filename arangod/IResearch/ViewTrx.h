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
#include "IResearch/IResearchLink.h"

namespace arangodb::iresearch {

//////////////////////////////////////////////////////////////////////////////
/// @brief a snapshot representation of the view with ability to query for cid
//////////////////////////////////////////////////////////////////////////////
class ViewSnapshot : public irs::index_reader {
 public:
  // @return cid of the sub-reader at operator['offset'] or 0 if undefined
  virtual DataSourceId cid(size_t offset) const noexcept = 0;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief index reader implementation over multiple irs::index_reader
///        the container storing the view state for a given TransactionState
/// @note it is assumed that DBServer ViewState resides in the same
///       TransactionState as the IResearchView ViewState, therefore a separate
///       lock is not required to be held by the DBServer CompoundReader
////////////////////////////////////////////////////////////////////////////////
class ViewTrxState final : public TransactionState::Cookie,
                           public ViewSnapshot {
 public:
  irs::sub_reader const& operator[](size_t subReaderId) const noexcept final {
    TRI_ASSERT(subReaderId < _subReaders.size());
    return *(_subReaders[subReaderId].second);
  }

  void add(DataSourceId cid, IResearchDataStore::Snapshot&& snapshot);

  DataSourceId cid(size_t offset) const noexcept final {
    return offset < _subReaders.size() ? _subReaders[offset].first
                                       : DataSourceId::none();
  }

  void clear() noexcept {
    _collections.clear();
    _subReaders.clear();
    _snapshots.clear();
    _live_docs_count = 0;
    _docs_count = 0;
  }

  bool equalCollections(
      containers::FlatHashSet<DataSourceId> const& collections) {
    return collections == _collections;
  }

  template<typename Collections>
  bool equalCollections(Collections const& collections) {
    if (collections.size() != _collections.size()) {
      return false;
    }
    for (auto const& cid : _collections) {
      if (!collections.contains(cid)) {
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] uint64_t docs_count() const noexcept final {
    return _docs_count;
  }
  [[nodiscard]] uint64_t live_docs_count() const noexcept final {
    return _live_docs_count;
  }
  [[nodiscard]] size_t size() const noexcept final {
    return _subReaders.size();
  }

 private:
  size_t _docs_count = 0;
  size_t _live_docs_count = 0;
  containers::FlatHashSet<DataSourceId> _collections;
  std::vector<IResearchLink::Snapshot> _snapshots;
  // prevent data-store deallocation (lock @ AsyncSelf)
  std::vector<std::pair<DataSourceId, irs::sub_reader const*>> _subReaders;
};

void ViewTrxState::add(DataSourceId cid,
                       IResearchDataStore::Snapshot&& snapshot) {
  auto& reader = snapshot.getDirectoryReader();
  for (auto& entry : reader) {
    _subReaders.emplace_back(std::piecewise_construct,
                             std::forward_as_tuple(cid),
                             std::forward_as_tuple(&entry));
  }
  _docs_count += reader.docs_count();
  _live_docs_count += reader.live_docs_count();
  _collections.emplace(cid);
  _snapshots.emplace_back(std::move(snapshot));
}

}  // namespace arangodb::iresearch

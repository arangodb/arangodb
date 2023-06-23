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
/// @author Markus Pfeiffer
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearch/IResearchDataStore.h"
#include "IResearchDataStore.h"

namespace arangodb::iresearch {

struct IResearchDataStoreHotbackupHelper : public IResearchDataStore {
  template<typename Link>
  IResearchDataStoreHotbackupHelper(std::string destinationPath,
                                    Link const& sourceDataStore)
      : IResearchDataStore(IResearchDataStore::DefaultConstructKey{}),
        _destinationPath(destinationPath),
        _sourceDataStore(sourceDataStore),
        _meta(&sourceDataStore.meta()) {
    _engine = sourceDataStore.engine();
    _asyncFeature = sourceDataStore.getIResearchFeature();
  }

  arangodb::Result initDataStore() {
    return std::visit(
        overload{[&](iresearch::IResearchInvertedIndexMeta const* meta) {
                   return initDataStore(
                       _destinationPath, static_cast<uint32_t>(meta->_version),
                       not meta->_sort.empty(), meta->hasNested(),
                       meta->_storedValues.columns(),
                       meta->_sort.sortCompression(), _indexReaderOptions);
                 },
                 [&](iresearch::IResearchLinkMeta const* meta) {
                   return initDataStore(
                       _destinationPath, static_cast<uint32_t>(meta->_version),
                       not meta->_sort.empty(), meta->hasNested(),
                       meta->_storedValues.columns(), meta->_sortCompression,
                       _indexReaderOptions);
                 }

        },
        _meta);
  };

  arangodb::Result initDataStore(
      std::string path, uint32_t version, bool sorted, bool nested,
      std::span<const IResearchViewStoredValues::StoredColumn> storedColumns,
      irs::type_info::type_id primarySortCompression,
      irs::IndexReaderOptions const& readerOptions);

  void commitHotbackupTransaction(uint64_t tick) {
    recoveryCommit(tick);
    _dataStore._writer->Commit({.tick = tick});
  }
  void unload() { _dataStore.resetDataStore(); }

  void hotbackupInsert(uint64_t tick, LocalDocumentId documentId,
                       velocypack::Slice doc) {
    std::visit(
        overload{[&](iresearch::IResearchLinkMeta const* meta) {
                   IResearchDataStore::recoveryInsert<FieldIterator<FieldMeta>,
                                                      IResearchLinkMeta>(
                       tick, documentId, doc, *meta);
                 },
                 [&](IResearchInvertedIndexMeta const* meta) {
                   IResearchDataStore::recoveryInsert<
                       FieldIterator<IResearchInvertedIndexMetaIndexingContext>,
                       IResearchInvertedIndexMetaIndexingContext>(
                       tick, documentId, doc, *meta->_indexingContext);
                 }

        },
        _meta);
  }

  void hotbackupRemove(LocalDocumentId documentId) {
    IResearchDataStore::recoveryRemove(documentId);
  }

  [[nodiscard]] Index& index() noexcept final { TRI_ASSERT(false); }

  [[nodiscard]] Index const& index() const noexcept final {
    return _sourceDataStore.index();
  }
  [[nodiscard]] AnalyzerPool::ptr findAnalyzer(
      AnalyzerPool const& analyzer) const final {
    TRI_ASSERT(false);
    return {};
  }
  void invalidateQueryCache(TRI_vocbase_t*) final { TRI_ASSERT(false); }
  irs::Comparer const* getComparator() const noexcept final {
    TRI_ASSERT(false);
    return nullptr;
    //   return _sourceDataStore.getComparator();
  }

  std::string _destinationPath;
  IResearchDataStore const& _sourceDataStore;

  using Meta = std::variant<iresearch::IResearchInvertedIndexMeta const*,
                            iresearch::IResearchLinkMeta const*>;
  Meta _meta;
  irs::IndexReaderOptions _indexReaderOptions;
};

}  // namespace arangodb::iresearch

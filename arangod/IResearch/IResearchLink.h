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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Result.h"
#include "IResearch/IResearchDataStore.h"
#include "IResearch/IResearchVPackComparer.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

namespace arangodb::iresearch {

class IResearchView;

////////////////////////////////////////////////////////////////////////////////
/// @brief common base class for functionality required to link an ArangoDB
///        LogicalCollection with an IResearchView
////////////////////////////////////////////////////////////////////////////////
class IResearchLink : public IResearchDataStore {
 public:
  IResearchLink(IResearchLink const&) = delete;
  IResearchLink(IResearchLink&&) = delete;
  IResearchLink& operator=(IResearchLink const&) = delete;
  IResearchLink& operator=(IResearchLink&&) = delete;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is dropped
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Result drop();

  static bool isHidden();

  // IResearch does not provide a fixed default sort order
  static constexpr bool isSorted() noexcept { return false; }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief index comparator, used by the coordinator to detect if the
  /// specified
  ///        definition is the same as this link
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  bool matchesDefinition(velocypack::Slice slice) const;

  using IResearchDataStore::properties;
  //////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a jSON description of a IResearchLink object
  ///        elements are appended to an existing object
  //////////////////////////////////////////////////////////////////////////////
  Result properties(velocypack::Builder& builder, bool forPersistence) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is unloaded from memory
  /// @note arangodb::Index override
  ////////////////////////////////////////////////////////////////////////////////
  Result unload() noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief lookup referenced analyzer
  ////////////////////////////////////////////////////////////////////////////////
  AnalyzerPool::ptr findAnalyzer(AnalyzerPool const& analyzer) const override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize from the specified definition used in make(...)
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  Result init(velocypack::Slice definition, bool& pathExists,
              InitCallback const& init = {});

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get stored values
  ////////////////////////////////////////////////////////////////////////////////
  IResearchViewStoredValues const& storedValues() const noexcept;

  /// @brief sets the _collectionName in Link meta. Used in cluster only to
  /// store linked collection name (as shard name differs from the
  /// cluster-wide collection name)
  /// @param name collection to set. Should match existing value of the
  /// _collectionName if it is not empty.
  /// @return true if name not existed in link before and was actually set by
  /// this call, false otherwise
  bool setCollectionName(std::string_view name) noexcept;

  std::string const& getDbName() const noexcept;
  std::string const& getViewId() const noexcept;
  std::string getCollectionName() const;
  std::string const& getShardName() const noexcept;

  auto const& meta() const noexcept { return _meta; }

  void setBuilding(bool building) noexcept {
    _isBuilding.store(building, std::memory_order_relaxed);
  }
  [[nodiscard]] bool isBuilding() const noexcept {
    return _isBuilding.load(std::memory_order_relaxed);
  }

 protected:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief construct an uninitialized IResearch link, must call init(...)
  /// after
  ////////////////////////////////////////////////////////////////////////////////
  using IResearchDataStore::IResearchDataStore;

  void insertMetrics() final;
  void removeMetrics() final;

  void invalidateQueryCache(TRI_vocbase_t* vocbase) override;

  irs::Comparer const* getComparator() const noexcept final {
    return &_comparer;
  }

 private:
  template<typename T>
  Result toView(std::shared_ptr<LogicalView> const& logical,
                std::shared_ptr<T>& view);
  Result initAndLink(bool& pathExists, InitCallback const& init,
                     IResearchView* view);

  Result initSingleServer(bool& pathExists, InitCallback const& init);
  Result initCoordinator(InitCallback const& init);
  Result initDBServer(bool& pathExists, InitCallback const& init);

  IResearchLinkMeta _meta;
  // the identifier of the desired view (read-only, set via init())
  std::string _viewGuid;

  VPackComparer<IResearchViewSort> _comparer;

  std::atomic_bool _isBuilding{false};
};

}  // namespace arangodb::iresearch

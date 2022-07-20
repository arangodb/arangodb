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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchViewMeta.h"
#include "VocBase/LogicalView.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {

struct ViewFactory;

}  // namespace arangodb

namespace arangodb::iresearch {

class IResearchLink;

///////////////////////////////////////////////////////////////////////////////
/// @class IResearchViewCoordinator
/// @brief an abstraction over the distributed IResearch index implementing the
///        LogicalView interface
///////////////////////////////////////////////////////////////////////////////
class IResearchViewCoordinator final : public LogicalView {
 public:
  static constexpr std::pair<ViewType, std::string_view> typeInfo() noexcept {
    return {ViewType::kView, StaticStrings::ViewType};
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the factory for this type of view
  //////////////////////////////////////////////////////////////////////////////
  static arangodb::ViewFactory const& factory();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief acquire locks on the specified 'link' during read-transactions
  ///        allowing retrieval of documents contained in the aforementioned
  ///        collection
  /// @note definitions are not persisted
  /// @return the 'link' was newly added to the IResearch View
  //////////////////////////////////////////////////////////////////////////////
  Result link(IResearchLink const& link);

  void open() final {}

  using LogicalDataSource::properties;
  Result properties(VPackSlice properties, bool isUserRequest,
                    bool partialUpdate) final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief unlink remove 'cid' from the persisted list of tracked collection
  ///        IDs
  /// @return success == view does not track collection
  //////////////////////////////////////////////////////////////////////////////
  Result unlink(DataSourceId cid) noexcept;

  bool visitCollections(CollectionVisitor const& visitor) const final;

  ///////////////////////////////////////////////////////////////////////////////
  /// @return primary sorting order of a view, empty -> use system order
  ///////////////////////////////////////////////////////////////////////////////
  IResearchViewSort const& primarySort() const noexcept {
    return _meta._primarySort;
  }

  ///////////////////////////////////////////////////////////////////////////////
  /// @return primary sort column compression method
  ///////////////////////////////////////////////////////////////////////////////
  auto const& primarySortCompression() const noexcept {
    return _meta._primarySortCompression;
  }

  ///////////////////////////////////////////////////////////////////////////////
  /// @return stored values from links collections
  ///////////////////////////////////////////////////////////////////////////////
  IResearchViewStoredValues const& storedValues() const noexcept {
    return _meta._storedValues;
  }

 private:
  Result appendVPackImpl(VPackBuilder& build, Serialization ctx,
                         bool safe) const final;

  Result dropImpl() final;

  Result renameImpl(std::string const& oldName) final;

  struct ViewFactory;

  IResearchViewCoordinator(TRI_vocbase_t& vocbase, VPackSlice info);

  // transient member, not persisted
  std::unordered_map<DataSourceId, std::pair<std::string, VPackBuilder>>
      _collections;
  mutable std::shared_mutex _mutex;  // for use with '_collections'
  IResearchViewMeta _meta;
};

}  // namespace arangodb::iresearch

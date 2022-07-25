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

#include "VocBase/LogicalView.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchDataStore.h"
#include "IResearch/ViewSnapshot.h"

#include <shared_mutex>
#include <atomic>

namespace arangodb {

struct ViewFactory;

}  // namespace arangodb
namespace arangodb::iresearch {

class SearchFactory;

class Search final : public LogicalView {
  using AsyncLinkPtr = std::shared_ptr<AsyncLinkHandle>;

 public:
  static constexpr std::pair<ViewType, std::string_view> typeInfo() noexcept {
    return {ViewType::kSearch, StaticStrings::SearchType};
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the factory for this type of view
  //////////////////////////////////////////////////////////////////////////////
  static ViewFactory const& factory();

  Search(TRI_vocbase_t& vocbase, velocypack::Slice info);
  ~Search();

  /// Get from indexes

  //////////////////////////////////////////////////////////////////////////////
  /// TODO
  //////////////////////////////////////////////////////////////////////////////
  IResearchInvertedIndexSort const& primarySort() const;

  //////////////////////////////////////////////////////////////////////////////
  /// TODO
  //////////////////////////////////////////////////////////////////////////////
  IResearchViewStoredValues const& storedValues() const;

  /// Single Server only methods

  //////////////////////////////////////////////////////////////////////////////
  /// @brief apply any changes to 'trx' required by this view
  /// @return success
  //////////////////////////////////////////////////////////////////////////////
  bool apply(transaction::Methods& trx);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get all indexes
  //////////////////////////////////////////////////////////////////////////////
  ViewSnapshot::Links getLinks() const;

  /// LogicalView

  //////////////////////////////////////////////////////////////////////////////
  /// @brief updates properties of an existing DataSource
  /// @param definition the properties being updated
  /// @param isUserRequest not used TODO(MBkkt) remove it
  /// @param partialUpdate modify only the specified properties (false == all)
  //////////////////////////////////////////////////////////////////////////////
  Result properties(velocypack::Slice definition, bool isUserRequest,
                    bool partialUpdate) final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief opens an existing view when the server is restarted
  //////////////////////////////////////////////////////////////////////////////
  void open() final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief invoke visitor on all collections that a view will return
  /// @return visitation was successful
  //////////////////////////////////////////////////////////////////////////////
  bool visitCollections(CollectionVisitor const& visitor) const final;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief queries properties of an existing view
  //////////////////////////////////////////////////////////////////////////////
  Result appendVPackImpl(velocypack::Builder& build, Serialization ctx,
                         bool safe) const final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drop implementation-specific parts of an existing view
  ///        including persisted properties
  //////////////////////////////////////////////////////////////////////////////
  Result dropImpl() final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief renames implementation-specific parts of an existing view
  ///        including persistence of properties
  //////////////////////////////////////////////////////////////////////////////
  Result renameImpl(std::string const& oldName) final;

  using Indexes =
      containers::FlatHashMap<DataSourceId,
                              containers::SmallVector<AsyncLinkPtr, 1>>;
  Indexes _indexes;
  mutable std::shared_mutex _mutex;

  using AsyncSearchPtr = std::shared_ptr<AsyncValue<Search>>;
  // 'this' for the lifetime of the view (for use with asynchronous calls)
  // AsyncSearchPtr may be nullptr on single-server if link did not come up yet
  AsyncSearchPtr _asyncSelf;
  std::function<void(transaction::Methods& trx, transaction::Status status)>
      _trxCallback;  // for snapshot(...)
  // std::atomic_bool _inRecovery{false};

  friend SearchFactory;
};

}  // namespace arangodb::iresearch

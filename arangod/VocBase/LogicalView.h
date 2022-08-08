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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Auth/Common.h"
#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Basics/ReadWriteLock.h"
#include "Containers/SmallVector.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Meta/utility.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/LogicalDataSource.h"
#include "VocBase/voc-types.h"

#include <velocypack/Buffer.h>
#include <functional>
#include <span>

namespace arangodb {
namespace velocypack {

class Builder;
class Slice;

}  // namespace velocypack

class LogicalView : public LogicalDataSource {
 public:
  using ptr = std::shared_ptr<LogicalView>;
  using Indexes = containers::SmallVector<IndexId, 1>;
  // TODO(MBkkt) fu2::function_view
  // visitor for map<CollectionId, set<IndexId>>, Indexes is movable
  using CollectionVisitor = std::function<bool(DataSourceId, Indexes*)>;

  static constexpr Category category() noexcept { return Category::kView; }

  using LogicalDataSource::properties;

  ViewType type() const noexcept { return _typeInfo.first; }

  std::string_view typeName() const noexcept { return _typeInfo.second; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief updates properties of an existing DataSource
  /// @param definition the properties being updated
  /// @param isUserRequest TODO what?
  /// @param partialUpdate modify only the specified properties (false == all)
  //////////////////////////////////////////////////////////////////////////////
  virtual Result properties(velocypack::Slice definition, bool isUserRequest,
                            bool partialUpdate) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief queries properties of an existing view
  //////////////////////////////////////////////////////////////////////////////
  Result appendVPack(velocypack::Builder& build, Serialization ctx,
                     bool safe) const final;

  //////////////////////////////////////////////////////////////////////////////
  /// @return the current view is granted 'level' access
  //////////////////////////////////////////////////////////////////////////////
  bool canUse(auth::Level const& level);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a new view according to a definition
  /// @param view out-param for created view on success
  ///        on success non-null, on failure undefined
  /// @param vocbase database where the view resides
  /// @param definition the view definition
  /// @param isUserRequest creation request is coming from a user
  /// @return success and sets 'view' or failure
  //////////////////////////////////////////////////////////////////////////////
  static Result create(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                       velocypack::Slice definition, bool isUserRequest);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drop an existing view
  //////////////////////////////////////////////////////////////////////////////
  Result drop() final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief calls the callback on every view found for the specified vocbase
  /// @param callback if false is returned then enumiration stops
  /// @return full enumeration finished successfully
  //////////////////////////////////////////////////////////////////////////////
  static bool enumerate(
      TRI_vocbase_t& vocbase,
      std::function<bool(std::shared_ptr<LogicalView> const&)> const& callback);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief instantiates an existing view according to a definition
  /// @param vocbase database where the view resides
  /// @param definition the view definition
  /// @return view instance or nullptr on error
  //////////////////////////////////////////////////////////////////////////////
  static Result instantiate(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                            velocypack::Slice definition);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief opens an existing view when the server is restarted
  //////////////////////////////////////////////////////////////////////////////
  virtual void open() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief renames an existing view
  //////////////////////////////////////////////////////////////////////////////
  Result rename(std::string&& newName) final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief invoke visitor on all collections that a view will return
  /// @return visitation was successful
  //////////////////////////////////////////////////////////////////////////////
  virtual bool visitCollections(CollectionVisitor const& visitor) const = 0;

 protected:
  template<typename Impl, typename... Args>
  explicit LogicalView(Impl const& /*self*/, Args&&... args)
      : LogicalView{Impl::typeInfo(), std::forward<Args>(args)...} {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief queries properties of an existing view
  //////////////////////////////////////////////////////////////////////////////
  virtual Result appendVPackImpl(velocypack::Builder& build, Serialization ctx,
                                 bool safe) const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drop implementation-specific parts of an existing view
  ///        including persisted properties
  //////////////////////////////////////////////////////////////////////////////
  virtual Result dropImpl() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief renames implementation-specific parts of an existing view
  ///        including persistance of properties
  //////////////////////////////////////////////////////////////////////////////
  virtual Result renameImpl(std::string const& oldName) = 0;

 private:
  LogicalView(std::pair<ViewType, std::string_view> typeInfo,
              TRI_vocbase_t& vocbase, velocypack::Slice definition);

  // TODO seems to be ugly
  friend struct ::TRI_vocbase_t;
  // ensure LogicalDataSource members (e.g. _deleted/_name) are not modified
  // asynchronously
  mutable basics::ReadWriteLock _lock;

  std::pair<ViewType, std::string_view> _typeInfo;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief a helper for ClusterInfo View operations
////////////////////////////////////////////////////////////////////////////////
namespace cluster_helper {

Result construct(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                 velocypack::Slice definition) noexcept;

Result drop(LogicalView const& view) noexcept;

Result properties(LogicalView const& view, bool safe) noexcept;

}  // namespace cluster_helper

////////////////////////////////////////////////////////////////////////////////
/// @brief a helper for StorageEngine View operations
////////////////////////////////////////////////////////////////////////////////
namespace storage_helper {

Result construct(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                 velocypack::Slice definition) noexcept;

Result drop(LogicalView const& view) noexcept;

Result properties(LogicalView const& view, bool safe) noexcept;

Result rename(LogicalView const& view, std::string const& oldName) noexcept;

}  // namespace storage_helper
}  // namespace arangodb

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_VOCBASE_LOGICAL_VIEW_H
#define ARANGOD_VOCBASE_LOGICAL_VIEW_H 1

#include "LogicalDataSource.h"
#include "Basics/Common.h"
#include "Basics/Result.h"
#include "Basics/ReadWriteLock.h"
#include "VocBase/voc-types.h"

#include <velocypack/Buffer.h>

namespace arangodb {

namespace velocypack {
class Slice;
class Builder;
}

////////////////////////////////////////////////////////////////////////////////
/// @class LogicalView
////////////////////////////////////////////////////////////////////////////////
class LogicalView : public LogicalDataSource {
 public:
  typedef std::function<bool(TRI_voc_cid_t)> CollectionVisitor;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief typedef for a LogicalView pre-commit callback
  ///        called before completing view creation
  ///        e.g. before persisting definition to filesystem
  //////////////////////////////////////////////////////////////////////////////
  typedef std::function<bool(
    std::shared_ptr<LogicalView>const& view // a pointer to the created view
  )> PreCommitCallback;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the category representing a logical view
  //////////////////////////////////////////////////////////////////////////////
  static Category const& category() noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates view according to a definition
  /// @param preCommit called before completing view creation (IFF returns true)
  ///                  e.g. before persisting definition to filesystem
  //////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<LogicalView> create(
    TRI_vocbase_t& vocbase,
    velocypack::Slice definition,
    bool isNew,
    uint64_t planVersion = 0,
    PreCommitCallback const& preCommit = PreCommitCallback() // called before
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief opens an existing view when the server is restarted
  //////////////////////////////////////////////////////////////////////////////
  virtual void open() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drop an existing view
  //////////////////////////////////////////////////////////////////////////////
  virtual arangodb::Result drop() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief renames an existing view
  //////////////////////////////////////////////////////////////////////////////
  virtual Result rename(
    std::string&& newName,
    bool doSync
  ) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief builds a VelocyPack representation of the node LogicalView
  //////////////////////////////////////////////////////////////////////////////
  virtual void toVelocyPack(
    velocypack::Builder& result,
    bool includeProperties,
    bool includeSystem
  ) const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief updates properties of an existing view
  //////////////////////////////////////////////////////////////////////////////
  virtual arangodb::Result updateProperties(
    velocypack::Slice const& properties,
    bool partialUpdate,
    bool doSync
  ) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief invoke visitor on all collections that a view will return
  /// @return visitation was successful
  //////////////////////////////////////////////////////////////////////////////
  virtual bool visitCollections(CollectionVisitor const& visitor) const = 0;

 protected:
  LogicalView(
    TRI_vocbase_t& vocbase,
    velocypack::Slice const& definition,
    uint64_t planVersion
  );

 private:
  // FIXME seems to be ugly
  friend struct ::TRI_vocbase_t;

  // ensure LogicalDataSource members (e.g. _deleted/_name) are not modified asynchronously
  mutable basics::ReadWriteLock _lock;
}; // LogicalView

////////////////////////////////////////////////////////////////////////////////
/// @class DBServerLogicalView
////////////////////////////////////////////////////////////////////////////////
class DBServerLogicalView : public LogicalView {
 public:
  ~DBServerLogicalView() override;

  arangodb::Result drop() override final;

  Result rename(
    std::string&& newName,
    bool doSync
  ) override final;

  void toVelocyPack(
    velocypack::Builder& result,
    bool includeProperties,
    bool includeSystem
  ) const override final;

  arangodb::Result updateProperties(
    velocypack::Slice const& properties,
    bool partialUpdate,
    bool doSync
  ) override final;

 protected:
  DBServerLogicalView(
    TRI_vocbase_t& vocbase,
    velocypack::Slice const& definition,
    uint64_t planVersion
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief called by view factories during view creation to persist the view
  ///        to the storage engine
  //////////////////////////////////////////////////////////////////////////////
  static arangodb::Result create(DBServerLogicalView const& view);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drop implementation-specific parts of an existing view
  //////////////////////////////////////////////////////////////////////////////
  virtual arangodb::Result dropImpl() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a jSON description of a View object implementation
  //////////////////////////////////////////////////////////////////////////////
  virtual void getPropertiesVPack(
    velocypack::Builder& builder,
    bool forPersistence
  ) const = 0;

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief called when a view's properties are updated (i.e. delta-modified)
  ///////////////////////////////////////////////////////////////////////////////
  virtual arangodb::Result updateProperties(
    velocypack::Slice const& slice,
    bool partialUpdate
  ) = 0;
}; // LogicalView

}  // namespace arangodb

#endif

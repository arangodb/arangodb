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
  /// @brief creates view accoridn to a definition
  //////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<LogicalView> create(
    TRI_vocbase_t& vocbase,
    velocypack::Slice definition,
    bool isNew
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the category representing a logical view
  //////////////////////////////////////////////////////////////////////////////
  static Category const& category() noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief opens an existing view when the server is restarted
  //////////////////////////////////////////////////////////////////////////////
  virtual void open() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drop an existing view
  //////////////////////////////////////////////////////////////////////////////
  virtual void drop() = 0;

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
  LogicalView(TRI_vocbase_t* vocbase, velocypack::Slice const& definition);

 private:
  // FIXME seems to be ugly
  friend struct ::TRI_vocbase_t;

  // ensure LogicalDataSource members (e.g. _deleted/_name) are not modified asynchronously
  mutable basics::ReadWriteLock _lock;
}; // LogicalView

//////////////////////////////////////////////////////////////////////////////
/// @brief typedef for a LogicalView factory function
/// This typedef is used when registering the creator function for any view
/// type. the creator function is called when a view is first created or
/// re-opened after a server restart. the VelocyPack Slice will contain all
/// information about the view's general and implementation-specific properties.
/// the isNew flag will be true if the view is first created, and false if a
/// view is re-opened on a server restart.
//////////////////////////////////////////////////////////////////////////////
typedef std::function<std::shared_ptr<LogicalView>(
  TRI_vocbase_t& vocbase, // database
  arangodb::velocypack::Slice const& properties, // view properties
  bool isNew
)> ViewFactory;

////////////////////////////////////////////////////////////////////////////////
/// @class DBServerLogicalView
////////////////////////////////////////////////////////////////////////////////
class DBServerLogicalView : public LogicalView {
 public:
  ~DBServerLogicalView() override;

  void drop() override;

  void open() override;

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
    TRI_vocbase_t* vocbase,
    velocypack::Slice const& definition,
    bool isNew
  );

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

 private:
  bool _isNew;
}; // LogicalView

}  // namespace arangodb

#endif
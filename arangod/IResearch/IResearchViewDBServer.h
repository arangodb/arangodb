////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_IRESEARCH__IRESEARCH_VIEW_DBSERVER_H
#define ARANGOD_IRESEARCH__IRESEARCH_VIEW_DBSERVER_H 1

#include "utils/async_utils.hpp"

#include "IResearchView.h"
#include "Transaction/Status.h"
#include "velocypack/Builder.h"
#include "VocBase/LogicalView.h"

namespace arangodb {

class DatabasePathFeature;
class TransactionState;
struct ViewFactory; // forward declaration
class CollectionNameResolver;

namespace transaction {

class Methods; // forward declaration

} // transaction

} // arangodb

namespace arangodb {
namespace iresearch {

class AsyncMeta;

class IResearchViewDBServer final: public arangodb::LogicalViewClusterInfo {
 public:
  virtual ~IResearchViewDBServer() noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ensure there is a view instance for the specified 'cid'
  /// @param force creation of a new instance if none is available in vocbase
  /// @return an existing instance or create a new instance if none is registred
  ///         on ptr reset the view will be dropped if it has no collections
  /// @note view created in vocbase() to match callflow during regular startup
  //////////////////////////////////////////////////////////////////////////////
  std::shared_ptr<arangodb::LogicalView> ensure(
      TRI_voc_cid_t cid,
      bool create = true
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the factory for this type of view
  //////////////////////////////////////////////////////////////////////////////
  static arangodb::ViewFactory const& factory();

  virtual void open() override;

  virtual arangodb::Result properties(
    arangodb::velocypack::Slice const& properties,
    bool partialUpdate
  ) override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @return pointer to an index reader containing the datastore record snapshot
  ///         associated with 'state'
  ///         (nullptr == no view snapshot associated with the specified state)
  ///         if force == true && no snapshot -> associate current snapshot
  ////////////////////////////////////////////////////////////////////////////////
  irs::index_reader const* snapshot(
    transaction::Methods& trx,
    std::vector<std::string> const& shards,
    IResearchView::Snapshot mode = IResearchView::Snapshot::Find
  ) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief unlink remove 'cid' from the persisted list of tracked collection
  ///        IDs
  /// @return success == view does not track collection
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result unlink(TRI_voc_cid_t cid) noexcept;

  virtual bool visitCollections(
    CollectionVisitor const& visitor
  ) const override;

 protected:
  virtual arangodb::Result appendVelocyPackDetailed(
    arangodb::velocypack::Builder& builder,
    bool forPersistence
  ) const override;

  virtual arangodb::Result dropImpl() override;

 private:
  struct ViewFactory; // forward declaration

  std::map<TRI_voc_cid_t, std::shared_ptr<arangodb::LogicalView>> _collections;
  std::shared_ptr<AsyncMeta> _meta; // the shared view configuration (never null!!!)
  mutable irs::async_utils::read_write_mutex _mutex; // for use with members

  IResearchViewDBServer(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    arangodb::DatabasePathFeature const& dbPathFeature,
    uint64_t planVersion,
    std::shared_ptr<AsyncMeta> meta = nullptr
  );
};

} // iresearch
} // arangodb

#endif
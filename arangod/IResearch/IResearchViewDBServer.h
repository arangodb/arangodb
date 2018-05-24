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
#include "utils/memory.hpp"
#include "utils/utf8_path.hpp"
#include "velocypack/Builder.h"
#include "VocBase/LogicalView.h"

NS_BEGIN(arangodb)

class DatabasePathFeature; // forward declaration
class TransactionState; // forward declaration

NS_END // arangodb

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

class PrimaryKeyIndexReader; // forward declaration

class IResearchViewDBServer final: public arangodb::LogicalView {
 public:
  virtual ~IResearchViewDBServer();

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief apply any changes to 'state' required by this view
  ///////////////////////////////////////////////////////////////////////////////
  void apply(arangodb::TransactionState& state);

  virtual arangodb::Result drop() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief drop the view association for the specified 'cid'
  /// @return if an association was removed
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result drop(TRI_voc_cid_t cid);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ensure there is a view instance for the specified 'cid'
  /// @return an existing instance or create a new instance if none is registred
  ///         on ptr reset the view will be dropped if it has no collections
  /// @note view created in vocbase() to match callflow during regular startup
  //////////////////////////////////////////////////////////////////////////////
  std::shared_ptr<arangodb::LogicalView> ensure(TRI_voc_cid_t cid);

  ///////////////////////////////////////////////////////////////////////////////
  /// @brief view factory
  /// @returns initialized view object
  ///////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<LogicalView> make(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    bool isNew,
    uint64_t planVersion,
    LogicalView::PreCommitCallback const& preCommit = {}
  );

  virtual void open() override;
  virtual arangodb::Result rename(std::string&& newName, bool doSync) override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @return pointer to an index reader containing the datastore record snapshot
  ///         associated with 'state'
  ///         (nullptr == no view snapshot associated with the specified state)
  ///         if force == true && no snapshot -> associate current snapshot
  ////////////////////////////////////////////////////////////////////////////////
  PrimaryKeyIndexReader* snapshot(
    TransactionState& state,
    bool force = false
  ) const;

  virtual arangodb::Result updateProperties(
    arangodb::velocypack::Slice const& properties,
    bool partialUpdate,
    bool doSync
  ) override;
  virtual bool visitCollections(
    CollectionVisitor const& visitor
  ) const override;

 protected:
  virtual arangodb::Result appendVelocyPack(
    arangodb::velocypack::Builder& builder,
    bool detailed,
    bool forPersistence
  ) const override;

 private:
  DECLARE_SPTR(LogicalView);

  std::map<TRI_voc_cid_t, std::shared_ptr<arangodb::LogicalView>> _collections;
  arangodb::velocypack::Builder _meta; // the view definition
  mutable irs::async_utils::read_write_mutex _mutex; // for use with members
  irs::utf8_path const _persistedPath;
  std::function<void(arangodb::TransactionState& state)> _trxReadCallback; // for snapshot(...)

  IResearchViewDBServer(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    arangodb::DatabasePathFeature const& dbPathFeature,
    uint64_t planVersion
  );
};

NS_END // iresearch
NS_END // arangodb

#endif
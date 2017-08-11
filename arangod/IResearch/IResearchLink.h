//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_LINK_H
#define ARANGOD_IRESEARCH__IRESEARCH_LINK_H 1

#include "IResearchLinkMeta.h"
#include "IResearchView.h"

#include "Indexes/Index.h"

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

class IResearchLink final: public Index {
 public:
  DECLARE_SPTR(IResearchLink);

  virtual ~IResearchLink();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief does this iResearch Link reference the supplied view
  ////////////////////////////////////////////////////////////////////////////////
  bool operator==(IResearchView const& view) const noexcept;
  bool operator!=(IResearchView const& view) const noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief does this iResearch Link match the meta definition
  ////////////////////////////////////////////////////////////////////////////////
  bool operator==(IResearchLinkMeta const& meta) const noexcept;
  bool operator!=(IResearchLinkMeta const& meta) const noexcept;

  bool allowExpansion() const override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief insert a set of ArangoDB documents into an iResearch View using
  ///        '_meta' params
  ////////////////////////////////////////////////////////////////////////////////
  virtual void batchInsert(
    transaction::Methods* trx,
    std::vector<std::pair<TRI_voc_rid_t, arangodb::velocypack::Slice>> const& batch,
    std::shared_ptr<arangodb::basics::LocalTaskQueue> queue = nullptr
  ) override;

  bool canBeDropped() const override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is dropped
  ////////////////////////////////////////////////////////////////////////////////
  int drop() override;

  bool hasBatchInsert() const override;
  bool hasSelectivityEstimate() const override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief insert an ArangoDB document into an iResearch View using '_meta' params
  ////////////////////////////////////////////////////////////////////////////////
  Result insert(
    transaction::Methods* trx,
    TRI_voc_rid_t rid,
    VPackSlice const& doc,
    bool isRollback
  ) override;

  bool isPersistent() const override;
  bool isSorted() const override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief create and initialize an iResearch View Link instance
  /// @return nullptr on failure
  ////////////////////////////////////////////////////////////////////////////////
  static ptr make(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection* collection,
    VPackSlice const& definition
  ) noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief index comparator, used by the coordinator to detect if the specified
  ///        definition is the same as this link
  ////////////////////////////////////////////////////////////////////////////////
  bool matchesDefinition(VPackSlice const& slice) const override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this iResearch Link
  ////////////////////////////////////////////////////////////////////////////////
  size_t memory() const override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief remove an ArangoDB document from an iResearch View
  ////////////////////////////////////////////////////////////////////////////////
  Result remove(
    transaction::Methods* trx,
    TRI_voc_rid_t rid,
    VPackSlice const& doc,
    bool isRollback
  ) override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief set a flag in the builder to prevent registration with the
  ///        corresponding iResearch View during construction of the object
  ///        NOTE: required to avoid deadlock when looking-up view in vocbase
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  static bool setSkipViewRegistration(arangodb::velocypack::Builder& builder);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief set the iResearch link 'type' field in the builder to the proper
  ///        value
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  static bool setType(arangodb::velocypack::Builder& builder);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief set the iResearch view identifier field in the builder to the
  ///        specified value
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  static bool setView(
    arangodb::velocypack::Builder& builder,
    TRI_voc_cid_t value
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchLink object
  /// @param withFigures output 'figures' section with e.g. memory size
  ////////////////////////////////////////////////////////////////////////////////
  using Index::toVelocyPack; // for Index::toVelocyPack(bool)
  void toVelocyPack(
    VPackBuilder& builder,
    bool withFigures,
    bool forPeristence
  ) const override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief iResearch Link index type enum value
  ////////////////////////////////////////////////////////////////////////////////
  IndexType type() const override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief iResearch Link index type string value
  ////////////////////////////////////////////////////////////////////////////////
  char const* typeName() const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is loaded into memory
  ////////////////////////////////////////////////////////////////////////////////
  int load() override;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief called when the iResearch Link is unloaded from memory
  ////////////////////////////////////////////////////////////////////////////////
  int unload() override;

protected:
  friend bool IResearchView::linkRegister(IResearchLink&); // to call updateView(...)
  friend IResearchView::~IResearchView(); // to call updateView(...)
  TRI_voc_cid_t _defaultId; // the identifier of the desired view (iff _view == nullptr)
  IResearchLinkMeta _meta; // how this collection should be indexed
  mutable irs::async_utils::read_write_mutex _mutex; // for use with _view to allow asynchronous disassociation
  IResearchView::sptr _view; // effectively the index itself (nullptr == not associated)

  IResearchLink(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection* collection,
    IResearchLinkMeta&& meta
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief update the '_view' pointer under lock and return the original value
  /// @param isNew this is a new instance of the link for the view
  ////////////////////////////////////////////////////////////////////////////////
  IResearchView::sptr updateView(
      IResearchView::sptr const& view,
      bool isNew = false
  );
}; // IResearchLink

////////////////////////////////////////////////////////////////////////////////
/// @brief copy required fields from the 'definition' into the 'builder'
////////////////////////////////////////////////////////////////////////////////
int EnhanceJsonIResearchLink(
  VPackSlice const definition,
  VPackBuilder& builder,
  bool create
) noexcept;

NS_END // iresearch
NS_END // arangodb
#endif
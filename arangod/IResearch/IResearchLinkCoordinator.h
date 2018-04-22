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

#ifndef ARANGODB_IRESEARCH__IRESEARCH_LINK_COORDINATOR_H
#define ARANGODB_IRESEARCH__IRESEARCH_LINK_COORDINATOR_H 1

#include "Indexes/Index.h"
#include "IResearchLinkMeta.h"

namespace arangodb {
namespace iresearch {

class IResearchViewCoordinator;

////////////////////////////////////////////////////////////////////////////////
/// @brief common base class for functionality required to link an ArangoDB
///        LogicalCollection with an IResearchView on a coordinator in cluster
////////////////////////////////////////////////////////////////////////////////
class IResearchLinkCoordinator {
 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief finds first link between specified collection and view
  ////////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<IResearchLinkCoordinator> find(
    LogicalCollection const& collection,
    LogicalView const& view
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief factory method for MMFiles engine
  ////////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<Index> createLinkMMFiles(
    LogicalCollection* collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  ) noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief factory method for RocksDB engine
  ////////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<Index> createLinkRocksDB(
    LogicalCollection* collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  ) noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief destructor
  ////////////////////////////////////////////////////////////////////////////////
  virtual ~IResearchLinkCoordinator() = default;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief does this IResearch Link reference the supplied view
  ////////////////////////////////////////////////////////////////////////////////
  bool operator==(LogicalView const& view) const noexcept;
  bool operator!=(LogicalView const& view) const noexcept {
    return !(*this == view);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief does this iResearch Link match the meta definition
  ////////////////////////////////////////////////////////////////////////////////
  bool operator==(IResearchLinkMeta const& meta) const noexcept {
    return _meta == meta;
  }

  bool operator!=(IResearchLinkMeta const& meta) const noexcept {
    return !(*this == meta);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief drops underlying link
  ////////////////////////////////////////////////////////////////////////////////
  Result drop();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the identifier for this link
  ////////////////////////////////////////////////////////////////////////////////
  TRI_idx_iid_t id() const noexcept {
    return _id;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief index comparator, used by the coordinator to detect if the specified
  ///        definition is the same as this link
  ////////////////////////////////////////////////////////////////////////////////
  bool matchesDefinition(VPackSlice const& slice) const;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief amount of memory in bytes occupied by this iResearch Link
  ////////////////////////////////////////////////////////////////////////////////
  size_t memory() const noexcept {
    return _meta.memory();
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief the collection of this link
  ////////////////////////////////////////////////////////////////////////////////
  LogicalCollection const& collection() const noexcept {
    return *_collection;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief fill and return a JSON description of a IResearchLinkCoordinator object
  ////////////////////////////////////////////////////////////////////////////////
  bool toVelocyPack(
    arangodb::velocypack::Builder& builder
  ) const;

 protected:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief construct an uninitialized IResearch link, must call init(...) after
  ////////////////////////////////////////////////////////////////////////////////
  IResearchLinkCoordinator(
    TRI_idx_iid_t id,
    LogicalCollection* collection
  ) noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize from the specified definition
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  bool init(VPackSlice definition);

  IResearchLinkMeta _meta; // how this collection should be indexed
  arangodb::LogicalCollection const* _collection; // the linked collection
  TRI_idx_iid_t const _id; // the index identifier
  TRI_voc_cid_t _defaultId; // the identifier of the desired view (iff _view == nullptr)
  std::shared_ptr<IResearchViewCoordinator> _view; // effectively the IResearch view itself (nullptr == not associated)
}; // IResearchLinkCoordinator

} // iresearch
} // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_VIEW_LINK_COORDINATOR_H 


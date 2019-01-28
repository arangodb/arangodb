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

#ifndef ARANGODB_IRESEARCH__IRESEARCH_VIEW_COORDINATOR_H
#define ARANGODB_IRESEARCH__IRESEARCH_VIEW_COORDINATOR_H 1

#include "IResearch/IResearchViewMeta.h"
#include "VocBase/LogicalView.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {

struct ViewFactory;  // forward declaration

}  // namespace arangodb

namespace arangodb {
namespace iresearch {

///////////////////////////////////////////////////////////////////////////////
/// @class IResearchViewCoordinator
/// @brief an abstraction over the distributed IResearch index implementing the
///        LogicalView interface
///////////////////////////////////////////////////////////////////////////////
class IResearchViewCoordinator final : public arangodb::LogicalView {
 public:
  virtual ~IResearchViewCoordinator();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief acquire locks on the specified 'cid' during read-transactions
  ///        allowing retrieval of documents contained in the aforementioned
  ///        collection
  /// @note definitions are not persisted
  /// @param cid the collection ID to track
  /// @param key the key of the link definition for use in appendVelocyPack(...)
  /// @param value the link definition to use in appendVelocyPack(...)
  /// @return the 'cid' was newly added to the IResearch View
  ////////////////////////////////////////////////////////////////////////////////
  bool emplace(TRI_voc_cid_t cid, std::string const& key,
               arangodb::velocypack::Slice const& value);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the factory for this type of view
  //////////////////////////////////////////////////////////////////////////////
  static arangodb::ViewFactory const& factory();

  void open() override {
    // NOOP
  }

  using LogicalDataSource::properties;
  virtual arangodb::Result properties(velocypack::Slice const& properties,
                                      bool partialUpdate) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief unlink remove 'cid' from the persisted list of tracked collection
  ///        IDs
  /// @return success == view does not track collection
  //////////////////////////////////////////////////////////////////////////////
  arangodb::Result unlink(TRI_voc_cid_t cid) noexcept;

  bool visitCollections(CollectionVisitor const& visitor) const override;

 protected:
  virtual Result appendVelocyPackImpl(arangodb::velocypack::Builder& builder,
                                      bool detailed, bool forPersistence) const override;

  virtual arangodb::Result dropImpl() override;

  arangodb::Result renameImpl(std::string const& oldName) override;

 private:
  struct ViewFactory;  // forward declaration

  IResearchViewCoordinator(TRI_vocbase_t& vocbase, velocypack::Slice info, uint64_t planVersion);

  std::unordered_map<TRI_voc_cid_t, std::pair<std::string, arangodb::velocypack::Builder>> _collections;  // transient member, not persisted
  mutable irs::async_utils::read_write_mutex _mutex;  // for use with '_collections'
  IResearchViewMeta _meta;
};  // IResearchViewCoordinator

}  // namespace iresearch
}  // namespace arangodb

#endif  // ARANGODB_IRESEARCH__IRESEARCH_VIEW_COORDINATOR_H

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

#include "VocBase/LogicalView.h"
#include "IResearch/IResearchViewMeta.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

namespace arangodb {
namespace iresearch {

///////////////////////////////////////////////////////////////////////////////
/// @class IResearchViewCoordinator
/// @brief an abstraction over the distributed IResearch index implementing the
///        LogicalView interface
///////////////////////////////////////////////////////////////////////////////
class IResearchViewCoordinator final : public arangodb::LogicalView {
 public:
  ///////////////////////////////////////////////////////////////////////////////
  /// @brief view factory
  /// @returns initialized view object
  ///////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<LogicalView> make(
    TRI_vocbase_t& vocbase,
    velocypack::Slice const& info,
    bool isNew,
    uint64_t planVersion,
    LogicalView::PreCommitCallback const& preCommit
  );

  bool visitCollections(CollectionVisitor const& visitor) const override;

  void open() override {
    // NOOP
  }

  Result drop() override;

  // drops collection from a view
  Result drop(TRI_voc_cid_t cid);

  virtual Result rename(
      std::string&& /*newName*/,
      bool /*doSync*/
  ) override {
    // not supported in a cluster
    return { TRI_ERROR_NOT_IMPLEMENTED };
  }

  virtual arangodb::Result updateProperties(
    velocypack::Slice const& properties,
    bool partialUpdate,
    bool doSync
  ) override;

 protected:
  virtual Result appendVelocyPack(
      arangodb::velocypack::Builder& builder,
      bool detailed,
      bool forPersistence
  ) const override ;

 private:
  IResearchViewCoordinator(
    TRI_vocbase_t& vocbase, velocypack::Slice info, uint64_t planVersion
  );

  IResearchViewMeta _meta;
  IResearchViewMetaState _metaState;
  velocypack::Builder _links;

}; // IResearchViewCoordinator

} // iresearch
} // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_VIEW_COORDINATOR_H

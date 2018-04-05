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

namespace arangodb {
namespace iresearch {

///////////////////////////////////////////////////////////////////////////////
/// @class IResearchViewCoordinator
/// @brief an abstraction over the distributed IResearch index implementing the
///        LogicalView interface
///////////////////////////////////////////////////////////////////////////////
class IResearchViewCoordinator final: public arangodb::LogicalView {
 public:
  ///////////////////////////////////////////////////////////////////////////////
  /// @brief view factory
  /// @returns initialized view object
  ///////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<LogicalView> make(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    uint64_t planVersion
  );

  bool visitCollections(CollectionVisitor const& visitor) const override {
    return false;
  }

  void open() override { }

  void drop() override { }

  virtual Result rename(std::string&& newName, bool doSync) override {
    return { TRI_ERROR_NOT_IMPLEMENTED };
  }

  virtual void toVelocyPack(
    arangodb::velocypack::Builder& result,
    bool includeProperties = false,
    bool includeSystem = false
  ) const override {
    // FIXME: implement
  }

  virtual arangodb::Result updateProperties(
      arangodb::velocypack::Slice const& properties,
      bool partialUpdate,
      bool doSync
  ) override {
    return { TRI_ERROR_NOT_IMPLEMENTED };
  }
}; // IResearchViewCoordinator

} // iresearch
} // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_VIEW_COORDINATOR_H

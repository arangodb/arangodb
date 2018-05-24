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

#ifndef ARANGODB_IRESEARCH__IRESEARCH_LINK_HELPER_H
#define ARANGODB_IRESEARCH__IRESEARCH_LINK_HELPER_H 1

#include "VocBase/voc-types.h"
#include "Basics/Result.h"

namespace arangodb {

class LogicalView; // forward declaration

namespace velocypack {

class Slice;
class Builder;

} // velocypack

namespace iresearch {

struct IResearchLinkMeta;

struct IResearchLinkHelper {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return a reference to a static VPackSlice of an empty index
  ///        definition
  //////////////////////////////////////////////////////////////////////////////
  static velocypack::Slice const& emptyIndexSlice();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief validate and copy required fields from the 'definition' into
  ///        'normalized'
  //////////////////////////////////////////////////////////////////////////////
  static arangodb::Result normalize(
    arangodb::velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  );

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief IResearch Link index type string value
  ////////////////////////////////////////////////////////////////////////////////
  static std::string const& type() noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief updates the collections in 'vocbase' to match the specified
  ///        IResearchLink definitions
  /// @param modified set of modified collection IDs
  /// @param viewId the view to associate created links with
  /// @param links the link modification definitions, null link == link removal
  /// @param stale links to remove if there is no creation definition in 'links'
  //////////////////////////////////////////////////////////////////////////////
  static arangodb::Result updateLinks(
      std::unordered_set<TRI_voc_cid_t>& modified,
      TRI_vocbase_t& vocbase,
      arangodb::LogicalView& view,
      arangodb::velocypack::Slice const& links,
      std::unordered_set<TRI_voc_cid_t> const& stale = {}
  );

 private:
  IResearchLinkHelper() = delete;
}; // IResearchLinkHelper

} // iresearch
} // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_LINK_HELPER_H

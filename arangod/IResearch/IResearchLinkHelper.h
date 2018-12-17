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

class LogicalCollection; // forward declaration
class LogicalView; // forward declaration

namespace velocypack {

class Slice;
class Builder;

} // velocypack

namespace iresearch {

class IResearchLink; // forward declaration
struct IResearchLinkMeta;

struct IResearchLinkHelper {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return a reference to a static VPackSlice of an empty index
  ///        definition
  //////////////////////////////////////////////////////////////////////////////
  static velocypack::Slice const& emptyIndexSlice();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief compare two link definitions for equivalience if used to create a
  ///        link instance
  //////////////////////////////////////////////////////////////////////////////
  static bool equal(
    arangodb::velocypack::Slice const& lhs,
    arangodb::velocypack::Slice const& rhs
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finds link between specified collection and view with the given id
  //////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<IResearchLink> find(
    arangodb::LogicalCollection const& collection,
    TRI_idx_iid_t id
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finds first link between specified collection and view
  //////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<IResearchLink> find(
    arangodb::LogicalCollection const& collection,
    LogicalView const& view
  );

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
  /// @brief validate the link specifications for:
  ///        * valid link meta
  ///        * collection existence
  ///        * collection permissions
  ///        * valid link meta
  //////////////////////////////////////////////////////////////////////////////
  static arangodb::Result validateLinks(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& links
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief visits all links in a collection
  /// @return full visitation compleated
  //////////////////////////////////////////////////////////////////////////////
  static bool visit(
    arangodb::LogicalCollection const& collection,
    std::function<bool(IResearchLink& link)> const& visitor
  );

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

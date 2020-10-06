////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__IRESEARCH_LINK_HELPER_H
#define ARANGODB_IRESEARCH__IRESEARCH_LINK_HELPER_H 1

#include <memory>

#include <utils/type_id.hpp>
#include "Basics/Result.h"
#include "VocBase/Identifiers/DataSourceId.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/voc-types.h"

namespace arangodb {
namespace application_features {
class ApplicationServer;
}

class LogicalCollection;  // forward declaration
class LogicalView;        // forward declaration

namespace velocypack {

class Slice;
class Builder;

}  // namespace velocypack

namespace iresearch {

class IResearchLink;  // forward declaration
struct IResearchLinkMeta;
class IResearchViewSort;
class IResearchViewStoredValues;

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
  static bool equal(  // equal definition
      arangodb::application_features::ApplicationServer& server,
      arangodb::velocypack::Slice const& lhs,  // left hand side
      arangodb::velocypack::Slice const& rhs,   // right hand side
      irs::string_ref const& dbname
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finds link between specified collection and view with the given id
  //////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<IResearchLink> find(         // find link
      arangodb::LogicalCollection const& collection,  // collection to search
      IndexId id                                      // index to find
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finds first link between specified collection and view
  //////////////////////////////////////////////////////////////////////////////
  static std::shared_ptr<IResearchLink> find( // find link
    arangodb::LogicalCollection const& collection, // collection to search
    LogicalView const& view // link for view to find
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief validate and copy required fields from the 'definition' into
  ///        'normalized'
  /// @note missing analyzers will be created if exceuted on db-server
  /// @note engine == nullptr then SEGFAULT in Methods constructor during insert
  /// @note true == inRecovery() then AnalyzerFeature will not allow persistence
  //////////////////////////////////////////////////////////////////////////////
  static arangodb::Result normalize( // normalize definition
    arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
    arangodb::velocypack::Slice definition, // source definition
    bool isCreation, // definition for index creation
    TRI_vocbase_t const& vocbase, // index vocbase
    IResearchViewSort const* primarySort = nullptr,
    irs::type_info::type_id const* primarySortCompression = nullptr,
    IResearchViewStoredValues const* storedValues = nullptr,
    arangodb::velocypack::Slice idSlice = arangodb::velocypack::Slice() // id for normalized
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
  static arangodb::Result validateLinks( // validate links
    TRI_vocbase_t& vocbase, // link vocbase
    arangodb::velocypack::Slice const& links // links to validate
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief visits all links in a collection
  /// @return full visitation compleated
  //////////////////////////////////////////////////////////////////////////////
  static bool visit( // visit links
    arangodb::LogicalCollection const& collection, // collection to visit
    std::function<bool(IResearchLink& link)> const& visitor // visitor to call
  );

  //////////////////////////////////////////////////////////////////////////////
  /// @brief updates the collections in 'vocbase' to match the specified
  ///        IResearchLink definitions
  /// @param modified set of modified collection IDs
  /// @param view the view to associate created links with
  /// @param links the link modification definitions, null link == link removal
  /// @param stale links to remove if there is no creation definition in 'links'
  //////////////////////////////////////////////////////////////////////////////
  static arangodb::Result updateLinks(             // update links
      std::unordered_set<DataSourceId>& modified,  // modified cids
      arangodb::LogicalView& view,                 // modified view
      arangodb::velocypack::Slice const& links,    // link definitions to apply
      std::unordered_set<DataSourceId> const& stale = {}  // stale view links
  );

 private:
  IResearchLinkHelper() = delete;
}; // IResearchLinkHelper

} // iresearch
} // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_LINK_HELPER_H

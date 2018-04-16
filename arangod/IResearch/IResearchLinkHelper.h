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

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief set the iResearch link 'type' field in the builder to the proper
  ///        value
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  static bool setType(velocypack::Builder& builder);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief set the IResearch view identifier field in the builder to the
  ///        specified value
  /// @return success
  ////////////////////////////////////////////////////////////////////////////////
  static bool setView(velocypack::Builder& builder, TRI_voc_cid_t value);

  ////////////////////////////////////////////////////////////////////////////////
  /// @returns view field from a specified link definition, or none slice if
  ///          there is no such field
  ////////////////////////////////////////////////////////////////////////////////
  static velocypack::Slice getView(velocypack::Slice definition) noexcept;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief IResearch Link index type string value
  ////////////////////////////////////////////////////////////////////////////////
  static std::string const& type() noexcept;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief validate and copy required fields from the 'definition' into
  ///        'normalized'
  //////////////////////////////////////////////////////////////////////////////
  static arangodb::Result normalize(
    arangodb::velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  );

 private:
  IResearchLinkHelper() = delete;
}; // IResearchLinkHelper

} // iresearch
} // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_LINK_HELPER_H


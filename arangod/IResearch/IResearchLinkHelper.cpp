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

#include "IResearchLinkHelper.h"
#include "IResearchCommon.h"
#include "IResearchLinkMeta.h"
#include "IResearchFeature.h"

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief the string representing the link type
////////////////////////////////////////////////////////////////////////////////
static const std::string& LINK_TYPE =
    arangodb::iresearch::DATA_SOURCE_TYPE.name();

////////////////////////////////////////////////////////////////////////////////
/// @brief the name of the field in the iResearch Link definition denoting the
///        iResearch Link type
////////////////////////////////////////////////////////////////////////////////
static const std::string LINK_TYPE_FIELD("type");

////////////////////////////////////////////////////////////////////////////////
/// @brief the id of the field in the iResearch Link definition denoting the
///        corresponding iResearch View
////////////////////////////////////////////////////////////////////////////////
static const std::string VIEW_ID_FIELD("view");

}

namespace arangodb {
namespace iresearch {

/*static*/ VPackSlice const& IResearchLinkHelper::emptyIndexSlice() {
  static const struct EmptySlice {
    VPackBuilder _builder;
    VPackSlice _slice;
    EmptySlice() {
      VPackBuilder fieldsBuilder;

      fieldsBuilder.openArray();
      fieldsBuilder.close(); // empty array
      _builder.openObject();
      _builder.add("fields", fieldsBuilder.slice()); // empty array
      arangodb::iresearch::IResearchLinkHelper::setType(_builder); // the index type required by Index
      _builder.close(); // object with just one field required by the Index constructor
      _slice = _builder.slice();
    }
  } emptySlice;

  return emptySlice._slice;
}

/*static*/ arangodb::Result IResearchLinkHelper::normalize(
  arangodb::velocypack::Builder& normalized,
  velocypack::Slice definition,
  bool // isCreation
) {
  if (!normalized.isOpenObject()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid output buffer provided for IResearch link normalized definition generation")
    );
  }

  std::string error;
  IResearchLinkMeta meta;

  if (!meta.init(definition, error)) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("error parsing IResearch link parameters from json: ") + error
    );
  }

  IResearchLinkHelper::setType(normalized);

  // copy over IResearch View identifier
  if (definition.hasKey(VIEW_ID_FIELD)) {
    normalized.add(VIEW_ID_FIELD, definition.get(VIEW_ID_FIELD));
  }

  return meta.json(normalized)
    ? arangodb::Result()
    : arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("error generating IResearch link normalized definition")
      )
    ;
}

/*static*/ bool IResearchLinkHelper::setType(velocypack::Builder& builder) {
  if (!builder.isOpenObject()) {
    return false;
  }

  builder.add(LINK_TYPE_FIELD, velocypack::Value(LINK_TYPE));

  return true;
}

/*static*/ bool IResearchLinkHelper::setView(
    velocypack::Builder& builder,
    TRI_voc_cid_t value
) {
  if (!builder.isOpenObject()) {
    return false;
  }

  builder.add(VIEW_ID_FIELD, velocypack::Value(value));

  return true;
}

/*static*/ std::string const& IResearchLinkHelper::type() noexcept {
  return LINK_TYPE;
}

} // iresearch
} // arangodb

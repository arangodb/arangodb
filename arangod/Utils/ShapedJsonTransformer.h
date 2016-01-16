////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_UTILS_SHAPED_JSON_TRANSFORMER_H
#define ARANGOD_UTILS_SHAPED_JSON_TRANSFORMER_H 1

#include "Basics/Common.h"
#include "Basics/JsonHelper.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/document-collection.h"

class VocShaper;

////////////////////////////////////////////////////////////////////////////////
/// @brief Transforms a given df_marker into a JSON object.
///        This json object contains all internal attributes.
////////////////////////////////////////////////////////////////////////////////

arangodb::basics::Json TRI_ExpandShapedJson(
    VocShaper* shaper, arangodb::arango::CollectionNameResolver const* resolver,
    TRI_voc_cid_t const& cid, TRI_df_marker_t const* marker);

////////////////////////////////////////////////////////////////////////////////
/// @brief Transforms a given doc_mptr into a JSON object.
///        This json object contains all internal attributes.
////////////////////////////////////////////////////////////////////////////////

arangodb::basics::Json TRI_ExpandShapedJson(
    VocShaper* shaper, arangodb::arango::CollectionNameResolver const* resolver,
    TRI_voc_cid_t const& cid, TRI_doc_mptr_t const* mptr);

#endif



////////////////////////////////////////////////////////////////////////////////
/// @brief transformer for shapedJson to Json
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Utils/ShapedJsonTransformer.h"
#include "VocBase/document-collection.h"
#include "VocBase/VocShaper.h"

using Json = triagens::basics::Json;
using CollectionNameResolver = triagens::arango::CollectionNameResolver;

Json TRI_ExpandShapedJson (
  VocShaper* shaper,
  CollectionNameResolver const* resolver,
  TRI_voc_cid_t const& cid,
  TRI_df_marker_t const* marker
) {
  TRI_shaped_json_t shaped;
  TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, marker);
  Json json(shaper->memoryZone(), TRI_JsonShapedJson(shaper, &shaped));

  // append the internal attributes

  // _id, _key, _rev
  char const* key = TRI_EXTRACT_MARKER_KEY(marker);
  std::string id(resolver->getCollectionName(cid));
  id.push_back('/');
  id.append(key);
  json(TRI_VOC_ATTRIBUTE_ID, Json(id));
  json(TRI_VOC_ATTRIBUTE_REV, Json(std::to_string(TRI_EXTRACT_MARKER_RID(marker))));
  json(TRI_VOC_ATTRIBUTE_KEY, Json(key));

  if (TRI_IS_EDGE_MARKER(marker)) {
    // _from
    std::string from(resolver->getCollectionNameCluster(TRI_EXTRACT_MARKER_FROM_CID(marker)));
    from.push_back('/');
    from.append(TRI_EXTRACT_MARKER_FROM_KEY(marker));
    json(TRI_VOC_ATTRIBUTE_FROM, Json(from));
    
    // _to
    std::string to(resolver->getCollectionNameCluster(TRI_EXTRACT_MARKER_TO_CID(marker)));

    to.push_back('/');
    to.append(TRI_EXTRACT_MARKER_TO_KEY(marker));
    json(TRI_VOC_ATTRIBUTE_TO, Json(to));
  }

  return json;
}

Json TRI_ExpandShapedJson (
  VocShaper* shaper,
  CollectionNameResolver const* resolver,
  TRI_voc_cid_t const& cid,
  TRI_doc_mptr_t const* mptr
) {
  TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(mptr->getDataPtr());
  return TRI_ExpandShapedJson(shaper, resolver, cid, marker);
}


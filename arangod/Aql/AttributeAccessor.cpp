////////////////////////////////////////////////////////////////////////////////
/// @brief AQL, specialized attribute accessor for expressions
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "AttributeAccessor.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/Variable.h"
#include "Basics/StringBuffer.h"
#include "Basics/json.h"
#include "VocBase/document-collection.h"
#include "VocBase/shaped-json.h"
#include "VocBase/VocShaper.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the accessor
////////////////////////////////////////////////////////////////////////////////

AttributeAccessor::AttributeAccessor (std::vector<char const*> const& attributeParts,
                                      Variable const* variable)
  : _attributeParts(attributeParts),
    _combinedName(),
    _variable(variable),
    _buffer(TRI_UNKNOWN_MEM_ZONE),
    _shaper(nullptr),
    _pid(0),
    _nameCache({ "", 0 }),
    _attributeType(ATTRIBUTE_TYPE_REGULAR) {

  TRI_ASSERT(_variable != nullptr);

  if (_attributeParts.size() == 1) {
    char const* n = _attributeParts[0];

    if (strcmp(n, TRI_VOC_ATTRIBUTE_KEY) == 0) {
      _attributeType = ATTRIBUTE_TYPE_KEY;
    }
    else if (strcmp(n, TRI_VOC_ATTRIBUTE_REV) == 0) {
      _attributeType = ATTRIBUTE_TYPE_REV;
    }
    else if (strcmp(n, TRI_VOC_ATTRIBUTE_ID) == 0) {
      _attributeType = ATTRIBUTE_TYPE_ID;
    }
    else if (strcmp(n, TRI_VOC_ATTRIBUTE_FROM) == 0) {
      _attributeType = ATTRIBUTE_TYPE_FROM;
    }
    else if (strcmp(n, TRI_VOC_ATTRIBUTE_TO) == 0) {
      _attributeType = ATTRIBUTE_TYPE_TO;
    }
  }

  if (_attributeType == ATTRIBUTE_TYPE_REGULAR) {
    for (auto const& it : _attributeParts) {
      if (! _combinedName.empty()) {
        _combinedName.push_back('.');
      }
      _combinedName.append(it);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the accessor
////////////////////////////////////////////////////////////////////////////////

AttributeAccessor::~AttributeAccessor () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the accessor
////////////////////////////////////////////////////////////////////////////////

AqlValue AttributeAccessor::get (triagens::arango::AqlTransaction* trx,
                                 AqlItemBlock const* argv,
                                 size_t startPos,
                                 std::vector<Variable const*> const& vars,
                                 std::vector<RegisterId> const& regs) {

  size_t i = 0;
  for (auto it = vars.begin(); it != vars.end(); ++it, ++i) {
    if ((*it)->id == _variable->id) {
      // get the AQL value
      auto& result = argv->getValueReference(startPos, regs[i]);
    
      // extract the attribute
      if (result.isShaped()) {
        switch (_attributeType) {
          case ATTRIBUTE_TYPE_KEY: {
            return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, TRI_EXTRACT_MARKER_KEY(result._marker)));
          }

          case ATTRIBUTE_TYPE_REV: {
            return extractRev(result);
          }

          case ATTRIBUTE_TYPE_ID: {
            TRI_document_collection_t const* collection = argv->getDocumentCollection(regs[i]);
            return extractId(result, trx, collection);
          }

          case ATTRIBUTE_TYPE_FROM: {
            return extractFrom(result, trx);
          }
          
          case ATTRIBUTE_TYPE_TO: {
            return extractTo(result, trx);
          }

          case ATTRIBUTE_TYPE_REGULAR:
          default: {
            TRI_document_collection_t const* collection = argv->getDocumentCollection(regs[i]);
            return extractRegular(result, trx, collection);
          }
        }
      }
      else if (result.isJson()) {
        TRI_json_t const* json = result._json->json();
        size_t const n = _attributeParts.size();
        size_t i = 0;

        while (TRI_IsObjectJson(json)) {
          TRI_ASSERT_EXPENSIVE(i < n);

          json = TRI_LookupObjectJson(json, _attributeParts[i]);

          if (json == nullptr) {
            break;
          }

          ++i;

          if (i == n) {
            // reached the end
            std::unique_ptr<TRI_json_t> copy(TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, json));
            
            if (copy == nullptr) {
              THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
            }

            auto result = new Json(TRI_UNKNOWN_MEM_ZONE, copy.get());
            copy.release();
            return AqlValue(result);
          }
        }

        // fall-through intentional
      }

      break;
    }
    // fall-through intentional
  }
  
  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _rev attribute from a ShapedJson marker
////////////////////////////////////////////////////////////////////////////////

AqlValue AttributeAccessor::extractRev (AqlValue const& src) {
  _buffer.reset();
  _buffer.appendInteger(TRI_EXTRACT_MARKER_RID(src._marker));

  auto json = new Json(TRI_UNKNOWN_MEM_ZONE, _buffer.c_str(), _buffer.length());

  return AqlValue(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _id attribute from a ShapedJson marker
////////////////////////////////////////////////////////////////////////////////

AqlValue AttributeAccessor::extractId (AqlValue const& src,
                                       triagens::arango::AqlTransaction* trx,
                                       TRI_document_collection_t const* document) {
  if (_nameCache.value.empty()) {
    _nameCache.value = trx->resolver()->getCollectionName(document->_info._cid);
  }

  _buffer.reset();
  _buffer.appendText(_nameCache.value);
  _buffer.appendChar('/');
  _buffer.appendText(TRI_EXTRACT_MARKER_KEY(src._marker));

  auto json = new Json(TRI_UNKNOWN_MEM_ZONE, _buffer.c_str(), _buffer.length());

  return AqlValue(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _from attribute from a ShapedJson marker
////////////////////////////////////////////////////////////////////////////////

AqlValue AttributeAccessor::extractFrom (AqlValue const& src,
                                         triagens::arango::AqlTransaction* trx) {
  if (src._marker->_type != TRI_DOC_MARKER_KEY_EDGE &&
      src._marker->_type != TRI_WAL_MARKER_EDGE) {
    return AqlValue(new Json(Json::Null));
  }
  
  auto cid = TRI_EXTRACT_MARKER_FROM_CID(src._marker);

  if (_nameCache.value.empty() || _nameCache.cid != cid) {
    _nameCache.cid = cid;
    _nameCache.value = trx->resolver()->getCollectionNameCluster(cid);
  }

  _buffer.reset();
  _buffer.appendText(_nameCache.value);
  _buffer.appendChar('/');
  _buffer.appendText(TRI_EXTRACT_MARKER_FROM_KEY(src._marker));
  
  auto json = new Json(TRI_UNKNOWN_MEM_ZONE, _buffer.c_str(), _buffer.length());

  return AqlValue(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _to attribute from a ShapedJson marker
////////////////////////////////////////////////////////////////////////////////

AqlValue AttributeAccessor::extractTo (AqlValue const& src,
                                       triagens::arango::AqlTransaction* trx) {
  if (src._marker->_type != TRI_DOC_MARKER_KEY_EDGE &&
      src._marker->_type != TRI_WAL_MARKER_EDGE) {
    return AqlValue(new Json(Json::Null));
  }

  auto cid = TRI_EXTRACT_MARKER_TO_CID(src._marker);
  
  if (_nameCache.value.empty() || _nameCache.cid != cid) {
    _nameCache.cid = cid;
    _nameCache.value = trx->resolver()->getCollectionNameCluster(cid);
  }

  _buffer.reset();
  _buffer.appendText(_nameCache.value);
  _buffer.appendChar('/');
  _buffer.appendText(TRI_EXTRACT_MARKER_TO_KEY(src._marker));
  
  auto json = new Json(TRI_UNKNOWN_MEM_ZONE, _buffer.c_str(), _buffer.length());

  return AqlValue(json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract any other attribute from a ShapedJson marker
////////////////////////////////////////////////////////////////////////////////

AqlValue AttributeAccessor::extractRegular (AqlValue const& src,
                                            triagens::arango::AqlTransaction* trx,
                                            TRI_document_collection_t const* document) {
  if (_shaper == nullptr) {
    _shaper = document->getShaper();
    _pid = _shaper->lookupAttributePathByName(_combinedName.c_str());
  }
   
  if (_pid != 0) { 
    // attribute exists
    TRI_ASSERT_EXPENSIVE(_shaper != nullptr);

    TRI_shaped_json_t shapedJson;
    TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, src._marker);

    TRI_shaped_json_t json;
    TRI_shape_t const* shape;

    bool ok = _shaper->extractShapedJson(&shapedJson, 0, _pid, &json, &shape);

    if (ok && shape != nullptr) {
      std::unique_ptr<TRI_json_t> extracted(TRI_JsonShapedJson(_shaper, &json));

      if (extracted == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }

      auto j = new Json(TRI_UNKNOWN_MEM_ZONE, extracted.get());
      extracted.release();
      return AqlValue(j);
    }
  }
    
  return AqlValue(new Json(Json::Null));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

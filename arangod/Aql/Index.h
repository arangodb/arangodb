////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, Index
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
/// @author not James
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_INDEX_H
#define ARANGODB_AQL_INDEX_H 1

#include "Basics/Common.h"
#include "Basics/json.h"
#include "Basics/JsonHelper.h"
#include "VocBase/index.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                      struct Index
// -----------------------------------------------------------------------------

    struct Index {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      Index& operator= (Index const&) = delete;
      
      Index (TRI_index_t* idx)
        : id(idx->_iid),
          type(idx->_type),
          unique(idx->_unique),
          fields(),
          data(idx) {

        size_t const n = idx->_fields._length;
        fields.reserve(n);

        for (size_t i = 0; i < n; ++i) {
          char const* field = idx->_fields._buffer[i];
          fields.push_back(std::string(field));
        }
        
        TRI_ASSERT(data != nullptr);
      }
      
      Index (TRI_json_t const* json)
        : id(triagens::basics::StringUtils::uint64(triagens::basics::JsonHelper::checkAndGetStringValue(json, "id"))),
          type(TRI_TypeIndex(triagens::basics::JsonHelper::checkAndGetStringValue(json, "type").c_str())),
          unique(triagens::basics::JsonHelper::checkAndGetBooleanValue(json, "unique")),
          fields(),
          data(nullptr) {

        TRI_json_t const* f = TRI_LookupArrayJson(json, "fields");

        if (TRI_IsListJson(f)) {
          size_t const n = TRI_LengthListJson(f);
          fields.reserve(n);

          for (size_t i = 0; i < n; ++i) {
            TRI_json_t const* name = static_cast<TRI_json_t const*>(TRI_AtVector(&f->_value._objects, i));
            if (TRI_IsStringJson(name)) {
              fields.push_back(std::string(name->_value._string.data, name->_value._string.length - 1));
            }
          }
        }
      }
      
      ~Index() {
      }
  
  
      triagens::basics::Json toJson () const {
        triagens::basics::Json json(triagens::basics::Json::Array);

        json("type", triagens::basics::Json(TRI_TypeNameIndex(type)))
            ("id", triagens::basics::Json(triagens::basics::StringUtils::itoa(id))) 
            ("unique", triagens::basics::Json(unique));

        triagens::basics::Json f(triagens::basics::Json::List);
        for (auto& field : fields) {
          f.add(triagens::basics::Json(field));
        }

        json("fields", f);
        return json;
      }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      TRI_idx_iid_t const        id;
      TRI_idx_type_e const       type;
      bool const                 unique;
      std::vector<std::string>   fields;
      TRI_index_t*               data;

    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

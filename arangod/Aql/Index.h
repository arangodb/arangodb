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
#include "Basics/Exceptions.h"
#include "Basics/json.h"
#include "Basics/JsonHelper.h"
#include "Indexes/Index.h"
#include "Indexes/PathBasedIndex.h"

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                      struct Index
// -----------------------------------------------------------------------------

    struct Index {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      Index (Index const&) = delete;
      Index& operator= (Index const&) = delete;
      
      Index (triagens::arango::Index* idx)
        : id(idx->id()),
          type(idx->type()),
          unique(false),
          sparse(false),
          fields(idx->fields()),
          internals(idx) {

        TRI_ASSERT(internals != nullptr);

        if (type == triagens::arango::Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
          unique = true;
        }
        else if (type == triagens::arango::Index::TRI_IDX_TYPE_HASH_INDEX ||
                 type == triagens::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX) {
          auto pathBasedIndex = static_cast<triagens::arango::PathBasedIndex const*>(idx);
          sparse = pathBasedIndex->sparse();
          unique = pathBasedIndex->unique();
        }
      }
      
      Index (TRI_json_t const* json)
        : id(triagens::basics::StringUtils::uint64(triagens::basics::JsonHelper::checkAndGetStringValue(json, "id"))),
          type(triagens::arango::Index::type(triagens::basics::JsonHelper::checkAndGetStringValue(json, "type").c_str())),
          unique(triagens::basics::JsonHelper::getBooleanValue(json, "unique", false)),
          sparse(triagens::basics::JsonHelper::getBooleanValue(json, "sparse", false)),
          fields(),
          internals(nullptr) {

        TRI_json_t const* f = TRI_LookupObjectJson(json, "fields");

        if (TRI_IsArrayJson(f)) {
          size_t const n = TRI_LengthArrayJson(f);
          fields.reserve(n);

          for (size_t i = 0; i < n; ++i) {
            auto * name = static_cast<TRI_json_t const*>(TRI_AtVector(&f->_value._objects, i));

            if (TRI_IsStringJson(name)) {
              std::vector<triagens::basics::AttributeName> parsedAttributes;
              TRI_ParseAttributeString(std::string(name->_value._string.data, name->_value._string.length - 1), parsedAttributes);
              fields.emplace_back(parsedAttributes);
            }
          }
        }

        // it is the caller's responsibility to fill the data attribute with something sensible later!
      }
      
      ~Index() {
      }
  
  
      triagens::basics::Json toJson () const {
        triagens::basics::Json json(triagens::basics::Json::Object);

        json("type", triagens::basics::Json(triagens::arango::Index::typeName(type)))
            ("id", triagens::basics::Json(triagens::basics::StringUtils::itoa(id))) 
            ("unique", triagens::basics::Json(unique))
            ("sparse", triagens::basics::Json(sparse));

        if (hasSelectivityEstimate()) {
          json("selectivityEstimate", triagens::basics::Json(selectivityEstimate()));
        }

        triagens::basics::Json f(triagens::basics::Json::Array);
        for (auto const& field : fields) {
          std::string tmp;
          TRI_AttributeNamesToString(field, tmp); 
          f.add(triagens::basics::Json(tmp));
        }

        json("fields", f);
        return json;
      }

      bool hasSelectivityEstimate () const {
        if (! hasInternals()) { 
          return false;
        }

        return getInternals()->hasSelectivityEstimate();
      }

      double selectivityEstimate () const {
        auto internals = getInternals();

        TRI_ASSERT(internals->hasSelectivityEstimate());

        return internals->selectivityEstimate();
      }
      
      inline bool hasInternals () const {
        return (internals != nullptr);
      }

      triagens::arango::Index* getInternals () const {
        if (internals == nullptr) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "accessing undefined index internals");
        }
        return internals; 
      }
      
      void setInternals (triagens::arango::Index* idx) {
        TRI_ASSERT(internals == nullptr);
        internals = idx;
      }

      bool canServeForConditionNode (triagens::aql::AstNode const* node,
                                     triagens::aql::Variable const* reference,
                                     std::vector<std::string> const* sortAttributes,
                                     double& estimatedCost) const {
        auto internals = getInternals();
        return internals->canServeForConditionNode(node, reference, sortAttributes, estimatedCost);
      }

      arango::IndexIterator* getIterator (triagens::aql::AstNode const* condition) const {
        auto internals = getInternals();
        // TODO: Convention: condition is fully evaluated and contains only
        // constant values
        return internals->iteratorForCondition(condition);
      }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      public:

        TRI_idx_iid_t const                                              id;
        triagens::arango::Index::IndexType                               type;
        bool                                                             unique;
        bool                                                             sparse;
        std::vector<std::vector<triagens::basics::AttributeName>>  fields;

      private:

        triagens::arango::Index*                      internals;

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

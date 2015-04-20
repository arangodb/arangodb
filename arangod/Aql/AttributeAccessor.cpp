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
#include "Aql/Variable.h"
#include "Basics/StringBuffer.h"
#include "Basics/json.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/document-collection.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the accessor
////////////////////////////////////////////////////////////////////////////////

AttributeAccessor::AttributeAccessor (char const* name,
                                      Variable const* variable)
  : _name(name),
    _variable(variable),
    _buffer(TRI_UNKNOWN_MEM_ZONE) {

  TRI_ASSERT(_name != nullptr);
  TRI_ASSERT(_variable != nullptr);
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
                                 std::vector<TRI_document_collection_t const*>& docColls,
                                 std::vector<AqlValue>& argv,
                                 size_t startPos,
                                 std::vector<Variable*> const& vars,
                                 std::vector<RegisterId> const& regs) {

  size_t i = 0;
  for (auto it = vars.begin(); it != vars.end(); ++it, ++i) {
    if ((*it)->id == _variable->id) {

      // save the collection info
      TRI_document_collection_t const* collection = docColls[regs[i]];

      // get the AQL value
      auto& result = argv[startPos + regs[i]];
    
      // extract the attribute
      auto j = result.extractObjectMember(trx, collection, _name, true, _buffer);
      return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, j.steal()));
    }
    // fall-through intentional
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

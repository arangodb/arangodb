////////////////////////////////////////////////////////////////////////////////
/// @brief storage for operations used for where statement
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
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "index-operator.h"
#include "VocBase/VocShaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

TRI_relation_index_operator_t::~TRI_relation_index_operator_t () {
  if (_parameters != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _parameters);
  }
  if (_fields != nullptr) {
    // _fields contains _numFields shapedJson objects
    for (size_t i = 0; i < _numFields; ++i) {
      // destroy each individual shapedJson object
      TRI_shaped_json_t* shaped = _fields + i;
      TRI_DestroyShapedJson(_shaper->memoryZone(), shaped);
    }
    // free the memory pointer
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, _fields);
  }
}



////////////////////////////////////////////////////////////////////////////////
/// @brief create a new index operator of the specified type
////////////////////////////////////////////////////////////////////////////////

TRI_index_operator_t* TRI_CreateIndexOperator (TRI_index_operator_type_e operatorType,
                                               TRI_index_operator_t* leftOperand,
                                               TRI_index_operator_t* rightOperand,
                                               TRI_json_t* parameters,
                                               VocShaper* shaper,
                                               size_t numFields) {
  switch (operatorType) {
    case TRI_AND_INDEX_OPERATOR:
    case TRI_NOT_INDEX_OPERATOR:
    case TRI_OR_INDEX_OPERATOR: {
      return new TRI_logical_index_operator_t(operatorType, shaper, leftOperand, rightOperand);
    }

    case TRI_EQ_INDEX_OPERATOR:
    case TRI_GE_INDEX_OPERATOR:
    case TRI_GT_INDEX_OPERATOR:
    case TRI_NE_INDEX_OPERATOR:
    case TRI_LE_INDEX_OPERATOR:
    case TRI_LT_INDEX_OPERATOR: {
      return new TRI_relation_index_operator_t(operatorType, shaper, parameters, nullptr, numFields);
    }

    default: {
      return nullptr;
    }
  } // end of switch statement
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

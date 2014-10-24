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

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new index operator of the specified type
////////////////////////////////////////////////////////////////////////////////

TRI_index_operator_t* TRI_CreateIndexOperator (TRI_index_operator_type_e operatorType,
                                               TRI_index_operator_t* leftOperand,
                                               TRI_index_operator_t* rightOperand,
                                               TRI_json_t* parameters,
                                               TRI_shaper_t* shaper,
                                               size_t numFields) {

  TRI_index_operator_t* newOperator;

  switch (operatorType) {
    case TRI_AND_INDEX_OPERATOR:
    case TRI_NOT_INDEX_OPERATOR:
    case TRI_OR_INDEX_OPERATOR: {
      TRI_logical_index_operator_t* newLogicalOperator = (TRI_logical_index_operator_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_logical_index_operator_t), false);

      if (newLogicalOperator == nullptr) {
        return nullptr;
      }

      newLogicalOperator->base._type    = operatorType;
      newLogicalOperator->base._shaper  = shaper;
      newLogicalOperator->_left         = leftOperand;
      newLogicalOperator->_right        = rightOperand;

      newOperator = &(newLogicalOperator->base);
      break;
    }

    case TRI_EQ_INDEX_OPERATOR:
    case TRI_GE_INDEX_OPERATOR:
    case TRI_GT_INDEX_OPERATOR:
    case TRI_NE_INDEX_OPERATOR:
    case TRI_LE_INDEX_OPERATOR:
    case TRI_LT_INDEX_OPERATOR: {
      TRI_relation_index_operator_t* newRelationOperator = (TRI_relation_index_operator_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_relation_index_operator_t), false);

      if (newRelationOperator == nullptr) {
        return nullptr;
      }

      newRelationOperator->base._type   = operatorType;
      newRelationOperator->base._shaper = shaper;
      newRelationOperator->_parameters  = parameters;
      newRelationOperator->_fields      = nullptr;
      newRelationOperator->_numFields   = numFields;

      newOperator = &(newRelationOperator->base);
      break;
    }

    default: {
      newOperator = nullptr;
      break;
    }
  } // end of switch statement

  return newOperator;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Destroys and frees any memory associated with an index operator
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearIndexOperator(TRI_index_operator_t* indexOperator) {
  if (indexOperator == nullptr) {
    return;
  }

  switch (indexOperator->_type) {
    case TRI_AND_INDEX_OPERATOR:
    case TRI_NOT_INDEX_OPERATOR:
    case TRI_OR_INDEX_OPERATOR: {
      TRI_logical_index_operator_t* logicalOperator = (TRI_logical_index_operator_t*) indexOperator;
      TRI_ClearIndexOperator(logicalOperator->_left);
      TRI_ClearIndexOperator(logicalOperator->_right);

      TRI_Free(TRI_UNKNOWN_MEM_ZONE, logicalOperator);
      break;
    }

    case TRI_EQ_INDEX_OPERATOR:
    case TRI_GE_INDEX_OPERATOR:
    case TRI_GT_INDEX_OPERATOR:
    case TRI_NE_INDEX_OPERATOR:
    case TRI_LE_INDEX_OPERATOR:
    case TRI_LT_INDEX_OPERATOR: {
      TRI_relation_index_operator_t* relationOperator = (TRI_relation_index_operator_t*) indexOperator;

      if (relationOperator->_parameters != nullptr) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, relationOperator->_parameters);
      }

      if (relationOperator->_fields != nullptr) {
        // relationOperator->_fields contains _numFields shapedJson objects
        for (size_t i = 0; i < relationOperator->_numFields; ++i) {
          // destroy each individual shapedJson object
          TRI_shaped_json_t* shaped = relationOperator->_fields + i;
          TRI_DestroyShapedJson(relationOperator->base._shaper->_memoryZone, shaped);
        }
        // free the memory pointer
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, relationOperator->_fields);
      }
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, relationOperator);
      break;
    }

  } // end of switch statement
}


////////////////////////////////////////////////////////////////////////////////
/// @brief copy a skiplist operator recursively (deep copy)
////////////////////////////////////////////////////////////////////////////////

TRI_index_operator_t* TRI_CopyIndexOperator (TRI_index_operator_t* indexOperator) {
  TRI_index_operator_t* newOperator = nullptr;

  if (indexOperator == nullptr) {
    return nullptr;
  }

  switch (indexOperator->_type) {
    case TRI_AND_INDEX_OPERATOR:
    case TRI_NOT_INDEX_OPERATOR:
    case TRI_OR_INDEX_OPERATOR: {
      TRI_logical_index_operator_t* oldLogicalOperator = (TRI_logical_index_operator_t*) indexOperator;
      TRI_logical_index_operator_t* newLogicalOperator = (TRI_logical_index_operator_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_logical_index_operator_t), false);

      if (newLogicalOperator == nullptr) { // out of memory ?
        break;
      }

      newLogicalOperator->base._type   = indexOperator->_type;
      newLogicalOperator->base._shaper = indexOperator->_shaper;
      newLogicalOperator->_left         = TRI_CopyIndexOperator(oldLogicalOperator->_left);
      newLogicalOperator->_right        = TRI_CopyIndexOperator(oldLogicalOperator->_right);
      newOperator = &(newLogicalOperator->base);

      break;

    }

    case TRI_EQ_INDEX_OPERATOR:
    case TRI_GE_INDEX_OPERATOR:
    case TRI_GT_INDEX_OPERATOR:
    case TRI_NE_INDEX_OPERATOR:
    case TRI_LE_INDEX_OPERATOR:
    case TRI_LT_INDEX_OPERATOR: {
      TRI_relation_index_operator_t* oldRelationOperator = (TRI_relation_index_operator_t*) indexOperator;
      TRI_relation_index_operator_t* newRelationOperator = (TRI_relation_index_operator_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_relation_index_operator_t), false);

      if (newRelationOperator == nullptr) { // out of memory?
        break;
      }

      newRelationOperator->base._type   = indexOperator->_type;
      newRelationOperator->base._shaper = indexOperator->_shaper;

      if (oldRelationOperator->_parameters != nullptr) {
        newRelationOperator->_parameters = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, oldRelationOperator->_parameters);
      }
      else {
        newRelationOperator->_parameters = nullptr;
      }

      if (oldRelationOperator->_fields != nullptr) {
        newRelationOperator->_fields = TRI_CopyShapedJson(oldRelationOperator->base._shaper, oldRelationOperator->_fields);
      }
      else {
        newRelationOperator->_fields = nullptr;
      }

      if (newRelationOperator->_fields != nullptr) {
        newRelationOperator->_numFields = oldRelationOperator->_numFields;
      }
      else {
        newRelationOperator->_numFields = 0;
      }

      newOperator = &(newRelationOperator->base);

      break;
    }
  }

  return newOperator;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief free a skiplist operator recursively
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeIndexOperator (TRI_index_operator_t* indexOperator) {
  TRI_ClearIndexOperator(indexOperator);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

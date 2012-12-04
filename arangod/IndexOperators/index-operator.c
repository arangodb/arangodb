////////////////////////////////////////////////////////////////////////////////
/// @brief storage for operations used for where statement
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. O
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "index-operator.h"


// -----------------------------------------------------------------------------
// --SECTION--                                                   SELECT DOCUMENT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
/// @brief create a new index operator of the specified type
////////////////////////////////////////////////////////////////////////////////

TRI_index_operator_t* TRI_CreateIndexOperator(TRI_index_operator_type_e operatorType,
                                              TRI_index_operator_t* leftOperand,
                                              TRI_index_operator_t* rightOperand,
                                              TRI_json_t* parameters,
                                              TRI_shaper_t* shaper,
                                              TRI_shaped_json_t* fields,
                                              size_t numFields, 
                                              void* collection) {

  TRI_index_operator_t*          newOperator;
  TRI_logical_index_operator_t*  newLogicalOperator;
  TRI_relation_index_operator_t* newRelationOperator;
  
  switch (operatorType) {
    case TRI_AND_INDEX_OPERATOR: 
    case TRI_NOT_INDEX_OPERATOR:
    case TRI_OR_INDEX_OPERATOR: {
      
      newLogicalOperator = (TRI_logical_index_operator_t*)TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_logical_index_operator_t), false);

      if (!newLogicalOperator) {
        return NULL;
      }

      newLogicalOperator->_base._type   = operatorType;
      newLogicalOperator->_base._shaper = shaper;
      newLogicalOperator->_left         = leftOperand;
      newLogicalOperator->_right        = rightOperand;
            
      newOperator = &(newLogicalOperator->_base);
      break;
    }
    
    case TRI_EQ_INDEX_OPERATOR: 
    case TRI_GE_INDEX_OPERATOR: 
    case TRI_GT_INDEX_OPERATOR: 
    case TRI_NE_INDEX_OPERATOR: 
    case TRI_LE_INDEX_OPERATOR: 
    case TRI_LT_INDEX_OPERATOR: {

      newRelationOperator = (TRI_relation_index_operator_t*)TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_relation_index_operator_t), false);

      if (!newRelationOperator) {
        return NULL;
      }

      newRelationOperator->_base._type = operatorType;
      newRelationOperator->_base._shaper = shaper;
      newRelationOperator->_parameters = parameters;
      newRelationOperator->_fields     = fields;
      newRelationOperator->_numFields  = numFields;
      newRelationOperator->_collection = collection;
      
      newOperator = &(newRelationOperator->_base);
      break;
    }    
    
    default: {
      newOperator = NULL;
      break;
    }      
  } // end of switch statement  
  
  return newOperator;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief Destroys and frees any memory associated with an index operator
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearIndexOperator(TRI_index_operator_t* indexOperator) {

  TRI_logical_index_operator_t*  logicalOperator;
  TRI_relation_index_operator_t* relationOperator;
  
  if (indexOperator == NULL) {
    return;
  }
  
  switch (indexOperator->_type) {
    case TRI_AND_INDEX_OPERATOR: 
    case TRI_NOT_INDEX_OPERATOR:
    case TRI_OR_INDEX_OPERATOR: {
    
      logicalOperator = (TRI_logical_index_operator_t*)(indexOperator);
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
    case TRI_LT_INDEX_OPERATOR:
    case TRI_IN_INDEX_OPERATOR: {
      relationOperator = (TRI_relation_index_operator_t*)(indexOperator);
      if (relationOperator->_parameters != NULL) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, relationOperator->_parameters);
      } 

      if (relationOperator->_fields != NULL) {
        size_t i;

        // relationOperator->_fields contains _numFields shapedJson objects
        for (i = 0; i < relationOperator->_numFields; ++i) {
          // destroy each individual shapedJson object
          TRI_shaped_json_t* shaped = relationOperator->_fields + i;
          TRI_DestroyShapedJson(relationOperator->_base._shaper, shaped);
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

TRI_index_operator_t* TRI_CopyIndexOperator(TRI_index_operator_t* indexOperator) {

  TRI_index_operator_t*          newOperator;
  TRI_logical_index_operator_t*  newLogicalOperator;
  TRI_logical_index_operator_t*  oldLogicalOperator;
  TRI_relation_index_operator_t* newRelationOperator;
  TRI_relation_index_operator_t* oldRelationOperator;
  
  newOperator = NULL;
  
  if (indexOperator == NULL) {
    return NULL;
  }
  
  switch (indexOperator->_type) {
    case TRI_AND_INDEX_OPERATOR: 
    case TRI_NOT_INDEX_OPERATOR:
    case TRI_OR_INDEX_OPERATOR: {    
    
      oldLogicalOperator = (TRI_logical_index_operator_t*)(indexOperator);
      newLogicalOperator = (TRI_logical_index_operator_t*) (TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_logical_index_operator_t), false));
      
      if (newLogicalOperator == NULL) { // out of memory ?
        break;
      }
      
      newLogicalOperator->_base._type   = indexOperator->_type;
      newLogicalOperator->_base._shaper = indexOperator->_shaper;
      newLogicalOperator->_left         = TRI_CopyIndexOperator(oldLogicalOperator->_left);
      newLogicalOperator->_right        = TRI_CopyIndexOperator(oldLogicalOperator->_right);
      newOperator = &(newLogicalOperator->_base);      

      break;
      
    }
    
    case TRI_EQ_INDEX_OPERATOR: 
    case TRI_GE_INDEX_OPERATOR: 
    case TRI_GT_INDEX_OPERATOR: 
    case TRI_NE_INDEX_OPERATOR: 
    case TRI_LE_INDEX_OPERATOR: 
    case TRI_LT_INDEX_OPERATOR: 
    case TRI_IN_INDEX_OPERATOR: {
    
      oldRelationOperator = (TRI_relation_index_operator_t*)(indexOperator);
      newRelationOperator = (TRI_relation_index_operator_t*) (TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_relation_index_operator_t), false));
      
      if (newRelationOperator == NULL) { // out of memory?
        break;
      }
      
      newRelationOperator->_base._type   = indexOperator->_type;
      newRelationOperator->_base._shaper = indexOperator->_shaper;
      
      if (oldRelationOperator->_parameters != NULL) {
        newRelationOperator->_parameters = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, oldRelationOperator->_parameters);
      }
      else {
        newRelationOperator->_parameters = NULL;
      }
      
      if (oldRelationOperator->_fields != NULL) {
        newRelationOperator->_fields = TRI_CopyShapedJson(oldRelationOperator->_base._shaper, oldRelationOperator->_fields);
      }  
      else {
        newRelationOperator->_fields = NULL;
      }  
      
      newRelationOperator->_numFields    = oldRelationOperator->_numFields;
      newRelationOperator->_collection   = oldRelationOperator->_collection;

      newOperator = &(newRelationOperator->_base);      
      
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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


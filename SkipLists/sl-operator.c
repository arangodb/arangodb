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

#include "sl-operator.h"


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
/// @brief create a new skiplist operator of the specified type
////////////////////////////////////////////////////////////////////////////////

TRI_sl_operator_t* TRI_CreateSLOperator(TRI_sl_operator_type_e operatorType,
                                        TRI_sl_operator_t* leftOperand,
                                        TRI_sl_operator_t* rightOperand,
                                        TRI_json_t* parameters,
                                        TRI_shaper_t* shaper,
                                        TRI_shaped_json_t* fields,
                                        size_t numFields, 
                                        void* collection) {

  TRI_sl_operator_t*          newOperator;
  TRI_sl_logical_operator_t*  newLogicalOperator;
  TRI_sl_relation_operator_t* newRelationOperator;
  
  switch (operatorType) {
    case TRI_SL_AND_OPERATOR: 
    case TRI_SL_NOT_OPERATOR:
    case TRI_SL_OR_OPERATOR: {
      
      newLogicalOperator = (TRI_sl_logical_operator_t*)TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_sl_logical_operator_t), false);

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
    
    case TRI_SL_EQ_OPERATOR: 
    case TRI_SL_GE_OPERATOR: 
    case TRI_SL_GT_OPERATOR: 
    case TRI_SL_NE_OPERATOR: 
    case TRI_SL_LE_OPERATOR: 
    case TRI_SL_LT_OPERATOR: {

      newRelationOperator = (TRI_sl_relation_operator_t*)TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_sl_relation_operator_t), false);

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
/// @brief Destroys and frees any memory associated with a Skiplist operator
////////////////////////////////////////////////////////////////////////////////

void TRI_ClearSLOperator(TRI_sl_operator_t* slOperator) {

  TRI_sl_logical_operator_t*  logicalOperator;
  TRI_sl_relation_operator_t* relationOperator;
  
  if (slOperator == NULL) {
    return;
  }
  
  switch (slOperator->_type) {
    case TRI_SL_AND_OPERATOR: 
    case TRI_SL_NOT_OPERATOR:
    case TRI_SL_OR_OPERATOR: {
    
      logicalOperator = (TRI_sl_logical_operator_t*)(slOperator);
      TRI_ClearSLOperator(logicalOperator->_left);
      TRI_ClearSLOperator(logicalOperator->_right);

      TRI_Free(TRI_UNKNOWN_MEM_ZONE, logicalOperator);
      break;
      
    }
    
    
    case TRI_SL_EQ_OPERATOR: 
    case TRI_SL_GE_OPERATOR: 
    case TRI_SL_GT_OPERATOR: 
    case TRI_SL_NE_OPERATOR: 
    case TRI_SL_LE_OPERATOR: 
    case TRI_SL_LT_OPERATOR: {
      size_t i;

      relationOperator = (TRI_sl_relation_operator_t*)(slOperator);
      if (relationOperator->_parameters != NULL) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, relationOperator->_parameters);
      } 

      if (relationOperator->_fields != NULL) {
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

TRI_sl_operator_t* TRI_CopySLOperator(TRI_sl_operator_t* slOperator) {

  TRI_sl_operator_t*          newOperator;
  
  newOperator = NULL;
  
  if (slOperator == NULL) {
    return NULL;
  }
  
  switch (slOperator->_type) {
    case TRI_SL_AND_OPERATOR: 
    case TRI_SL_NOT_OPERATOR:
    case TRI_SL_OR_OPERATOR: {
    
      TRI_sl_logical_operator_t*  newLogicalOperator;
      TRI_sl_logical_operator_t*  oldLogicalOperator;
    
      oldLogicalOperator = (TRI_sl_logical_operator_t*)(slOperator);
      newLogicalOperator = (TRI_sl_logical_operator_t*) (TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_sl_logical_operator_t), false));
      if (newLogicalOperator != NULL) {
        newLogicalOperator->_base._type   = slOperator->_type;
        newLogicalOperator->_base._shaper = slOperator->_shaper;
        newLogicalOperator->_left         = TRI_CopySLOperator(oldLogicalOperator->_left);
        newLogicalOperator->_right        = TRI_CopySLOperator(oldLogicalOperator->_right);
        newOperator = &(newLogicalOperator->_base);      
      }        
      break;
      
    }
    
    case TRI_SL_EQ_OPERATOR: 
    case TRI_SL_GE_OPERATOR: 
    case TRI_SL_GT_OPERATOR: 
    case TRI_SL_NE_OPERATOR: 
    case TRI_SL_LE_OPERATOR: 
    case TRI_SL_LT_OPERATOR: {
    
      TRI_sl_relation_operator_t* newRelationOperator;
      TRI_sl_relation_operator_t* oldRelationOperator;

      oldRelationOperator = (TRI_sl_relation_operator_t*)(slOperator);
      newRelationOperator = (TRI_sl_relation_operator_t*) (TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_sl_relation_operator_t), false));
      
      if (newRelationOperator == NULL) { // out of memory?
        break;
      }
      
      newRelationOperator->_base._type   = slOperator->_type;
      newRelationOperator->_base._shaper = slOperator->_shaper;
      
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

void TRI_FreeSLOperator (TRI_sl_operator_t* slOperator) {
  TRI_ClearSLOperator(slOperator);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


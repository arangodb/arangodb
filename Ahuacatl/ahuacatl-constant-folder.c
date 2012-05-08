////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, constant folding
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
/// @author Jan Steemann
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Ahuacatl/ahuacatl-constant-folder.h"
#include "Ahuacatl/ahuacatl-conversions.h"
#include "Ahuacatl/ahuacatl-functions.h"

#include "V8/v8-execution.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief hash a variable
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashFieldAccess (TRI_associative_pointer_t* array, 
                               void const* element) {
  TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) element;

  return TRI_FnvHashString(fieldAccess->_fieldName);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparison function used to determine variable equality
////////////////////////////////////////////////////////////////////////////////

static bool EqualFieldAccess (TRI_associative_pointer_t* array, 
                              void const* key, 
                              void const* element) {
  TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) element;

  return TRI_EqualString(key, fieldAccess->_fieldName);
}

#ifdef RANGE_OPTIMIZER
static char* AccessName (const TRI_aql_access_e type) {
  switch (type) {
    case TRI_AQL_ACCESS_ALL: 
      return "all";
    case TRI_AQL_ACCESS_IMPOSSIBLE: 
      return "impossible";
    case TRI_AQL_ACCESS_EXACT: 
      return "exact";
    case TRI_AQL_ACCESS_LIST: 
      return "list";
    case TRI_AQL_ACCESS_SINGLE_RANGE: 
      return "single range";
    case TRI_AQL_ACCESS_DOUBLE_RANGE: 
      return "double range";
    default:
      return "unknown";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free access member data
////////////////////////////////////////////////////////////////////////////////

static void FreeAccessMembers (TRI_aql_field_access_t* const fieldAccess) {
  assert(fieldAccess);

  switch (fieldAccess->_type) {
    case TRI_AQL_ACCESS_EXACT:
    case TRI_AQL_ACCESS_LIST:
      if (fieldAccess->_value._value) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_value._value);
      }
      break;
    case TRI_AQL_ACCESS_SINGLE_RANGE:
      if (fieldAccess->_value._singleRange._value) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_value._singleRange._value);
      }
      break;
    case TRI_AQL_ACCESS_DOUBLE_RANGE:
      if (fieldAccess->_value._between._lower._value) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_value._between._lower._value);
      }
      if (fieldAccess->_value._between._upper._value) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_value._between._upper._value);
      }
      break;

    case TRI_AQL_ACCESS_ALL:
    case TRI_AQL_ACCESS_IMPOSSIBLE:
    default: {
      // nada
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free access structure with its members
////////////////////////////////////////////////////////////////////////////////

static void FreeAccess (TRI_aql_field_access_t* const fieldAccess) {
  assert(fieldAccess);

  FreeAccessMembers(fieldAccess);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_fieldName);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldAccess);
}

static TRI_aql_field_access_t* MergeAccess (TRI_aql_context_t* const context,
                                            TRI_aql_field_access_t* lhs,
                                            TRI_aql_field_access_t* rhs) {
  assert(lhs);
  assert(rhs);

  assert(lhs->_fieldName != NULL);
  assert(rhs->_fieldName != NULL);

  if (lhs->_type > rhs->_type) {
    // swap operands so they are always sorted
    TRI_aql_field_access_t* tmp = lhs;
    lhs = rhs;
    rhs = tmp;
  }
  
  if (lhs->_type == TRI_AQL_ACCESS_IMPOSSIBLE) {
    // impossible merged with anything just returns impossible
    FreeAccess(rhs);

    return lhs;
  }

  if (lhs->_type == TRI_AQL_ACCESS_ALL) {
    // all merged with anything just returns the other side
    FreeAccess(lhs);

    return rhs;
  }
  

  if (lhs->_type == TRI_AQL_ACCESS_EXACT) {
    if (rhs->_type == TRI_AQL_ACCESS_EXACT) {
      // check if values are identical
      bool isSame = TRI_CheckSameValueJson(lhs->_value._value, rhs->_value._value);

      FreeAccess(rhs);

      if (!isSame) {
        // lhs and rhs values are non-identical, return impossible
        FreeAccessMembers(lhs);
        lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;
      }
      return lhs;
    }
    else if (rhs->_type == TRI_AQL_ACCESS_LIST) {
      // check if lhs is contained in rhs list
      bool inList = TRI_CheckInListJson(lhs->_value._value, rhs->_value._value);

      FreeAccess(rhs);

      if (!inList) {
        // lhs value is not in rhs list, return impossible
        FreeAccessMembers(lhs);
        lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;
      }

      return lhs;
    }
    else if (rhs->_type == TRI_AQL_ACCESS_SINGLE_RANGE) {
      // check if value is in range
      int result = TRI_CompareValuesJson(lhs->_value._value, rhs->_value._singleRange._value);
      
      bool contained = ((rhs->_value._singleRange._type == TRI_AQL_RANGE_LOWER_EXCLUDED && result > 0) ||
                        (rhs->_value._singleRange._type == TRI_AQL_RANGE_LOWER_INCLUDED && result >= 0) ||
                        (rhs->_value._singleRange._type == TRI_AQL_RANGE_UPPER_EXCLUDED && result < 0) ||
                        (rhs->_value._singleRange._type == TRI_AQL_RANGE_UPPER_INCLUDED && result <= 0));

      if (!contained) {
        // lhs value is not contained in rhs range, return impossible
        FreeAccess(rhs);
        FreeAccessMembers(lhs);
        lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

        return lhs;
      }

      // lhs value is contained in rhs range, simply return rhs
      FreeAccess(lhs);

      return rhs;
    }
    else if (rhs->_type == TRI_AQL_ACCESS_DOUBLE_RANGE) {
      // check if value is in range
      int result;
      bool contained;
      
      // compare lower end
      result = TRI_CompareValuesJson(lhs->_value._value, rhs->_value._between._lower._value);
      
      contained = ((rhs->_value._between._lower._type == TRI_AQL_RANGE_LOWER_EXCLUDED && result > 0) ||
                   (rhs->_value._between._lower._type == TRI_AQL_RANGE_LOWER_INCLUDED && result >= 0));

      if (!contained) {
        // lhs value is not contained in rhs range, return impossible
        FreeAccess(rhs);
        FreeAccessMembers(lhs);
        lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

        return lhs;
      }
 
      // compare upper end     
      result = TRI_CompareValuesJson(lhs->_value._value, rhs->_value._between._upper._value);
      
      contained = ((rhs->_value._between._upper._type == TRI_AQL_RANGE_UPPER_EXCLUDED && result < 0) ||
                   (rhs->_value._between._upper._type == TRI_AQL_RANGE_UPPER_INCLUDED && result <= 0));
      
      if (!contained) {
        // lhs value is not contained in rhs range, return impossible
        FreeAccess(rhs);
        FreeAccessMembers(lhs);
        lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

        return lhs;
      }

      // lhs value is contained in rhs range, return rhs
      FreeAccess(lhs);

      return rhs;
    }
  }

  if (lhs->_type == TRI_AQL_ACCESS_LIST) {
    if (rhs->_type == TRI_AQL_ACCESS_LIST) {
      // make a list of both
    }
    else if (rhs->_type == TRI_AQL_ACCESS_SINGLE_RANGE) {
      // check if value in range
    }
    else if (rhs->_type == TRI_AQL_ACCESS_DOUBLE_RANGE) {
      // check if value in range
    }
  }
  
  if (lhs->_type == TRI_AQL_ACCESS_SINGLE_RANGE) {
    if (rhs->_type == TRI_AQL_ACCESS_SINGLE_RANGE) {
      TRI_json_t* lhsValue;
      TRI_json_t* rhsValue;
      TRI_aql_range_e lhsType;
      TRI_aql_range_e rhsType;
      int result;

      if (lhs->_value._singleRange._type > rhs->_value._singleRange._type) {
        // swap operands so they are always sorted
        TRI_aql_field_access_t* tmp = lhs;
        lhs = rhs;
        rhs = tmp;
      }

      result = TRI_CompareValuesJson(lhs->_value._singleRange._value, rhs->_value._singleRange._value);
      lhsType = lhs->_value._singleRange._type;
      rhsType = rhs->_value._singleRange._type;
      lhsValue = lhs->_value._singleRange._value;
      rhsValue = rhs->_value._singleRange._value;

      // check if ranges overlap
      if ((lhsType == TRI_AQL_RANGE_LOWER_EXCLUDED && rhsType == TRI_AQL_RANGE_LOWER_EXCLUDED) ||
          (lhsType == TRI_AQL_RANGE_LOWER_INCLUDED && rhsType == TRI_AQL_RANGE_LOWER_INCLUDED)) {
        // > && >
        // >= && >=
        if (result > 0) {
          // lhs > rhs
          FreeAccess(rhs);

          return lhs;
        }
        else {
          FreeAccess(lhs);

          return rhs;
        }
      }
      else if ((lhsType == TRI_AQL_RANGE_UPPER_EXCLUDED && rhsType == TRI_AQL_RANGE_UPPER_EXCLUDED) ||
               (lhsType == TRI_AQL_RANGE_UPPER_INCLUDED && rhsType == TRI_AQL_RANGE_UPPER_INCLUDED)) {
        // < && <
        // <= && <=
        if (result > 0) {
          // lhs > rhs
          FreeAccess(lhs);

          return rhs;
        }
        else {
          FreeAccess(rhs);

          return lhs;
        }
      }
      else if (lhsType == TRI_AQL_RANGE_LOWER_EXCLUDED && rhsType == TRI_AQL_RANGE_LOWER_INCLUDED) {
        // > && >=
        if (result >= 0) {
          // lhs > rhs
          FreeAccess(rhs);

          return lhs;
        } 
        else {
          FreeAccess(lhs);

          return rhs;
        }
      }
      else if (lhsType == TRI_AQL_RANGE_LOWER_EXCLUDED && rhsType == TRI_AQL_RANGE_UPPER_EXCLUDED) {
        // > && <
        if (result < 0) {
          // save pointers
          lhsValue = lhs->_value._singleRange._value;
          rhsValue = rhs->_value._singleRange._value;
          rhs->_value._singleRange._value = NULL;
          FreeAccess(rhs); 

          lhs->_type = TRI_AQL_ACCESS_DOUBLE_RANGE;
          lhs->_value._between._lower._type = lhsType;
          lhs->_value._between._lower._value = lhsValue;
          lhs->_value._between._upper._type = rhsType;
          lhs->_value._between._upper._value = rhsValue;

          return lhs;
        }
        else {
          FreeAccess(rhs);
          FreeAccessMembers(lhs);
          lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

          return lhs;
        }
      }
      else if ((lhsType == TRI_AQL_RANGE_LOWER_EXCLUDED && rhsType == TRI_AQL_RANGE_UPPER_INCLUDED) ||
               (lhsType == TRI_AQL_RANGE_LOWER_INCLUDED && rhsType == TRI_AQL_RANGE_UPPER_EXCLUDED)) {
        // > && <=
        // >= && <
        if (result < 0) {
          // save pointers
          lhsValue = lhs->_value._singleRange._value;
          rhsValue = rhs->_value._singleRange._value;
          rhs->_value._singleRange._value = NULL;
          FreeAccess(rhs); 

          lhs->_type = TRI_AQL_ACCESS_DOUBLE_RANGE;
          lhs->_value._between._lower._type = lhsType;
          lhs->_value._between._lower._value = lhsValue;
          lhs->_value._between._upper._type = rhsType;
          lhs->_value._between._upper._value = rhsValue;

          return lhs;
        }
        else {
          FreeAccess(rhs);
          FreeAccessMembers(lhs);
          lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

          return lhs;
        }
      }
      else if (lhsType == TRI_AQL_RANGE_LOWER_INCLUDED && rhsType == TRI_AQL_RANGE_UPPER_INCLUDED) {
        // >= && <=
        if (result < 0) {
          // save pointers
          lhsValue = lhs->_value._singleRange._value;
          rhsValue = rhs->_value._singleRange._value;
          rhs->_value._singleRange._value = NULL;
          FreeAccess(rhs); 

          lhs->_type = TRI_AQL_ACCESS_DOUBLE_RANGE;
          lhs->_value._between._lower._type = lhsType;
          lhs->_value._between._lower._value = lhsValue;
          lhs->_value._between._upper._type = rhsType;
          lhs->_value._between._upper._value = rhsValue;

          return lhs;
        }
        else if (result == 0) {
          FreeAccess(rhs);

          // save pointer
          lhsValue = lhs->_value._singleRange._value;
          lhs->_type = TRI_AQL_ACCESS_EXACT;
          lhs->_value._value = lhsValue;
          return lhs;
        }
        else {
          FreeAccess(rhs);
          FreeAccessMembers(lhs);
          lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

          return lhs;
        }
      }
      else if (lhsType == TRI_AQL_RANGE_UPPER_EXCLUDED && rhsType == TRI_AQL_RANGE_UPPER_INCLUDED) {
        // < && <=
        if (result <= 0) {
          FreeAccess(rhs);

          return lhs;
        }
        else {
          FreeAccess(lhs);

          return rhs;
        }
      }
    }
    else if (rhs->_type == TRI_AQL_ACCESS_DOUBLE_RANGE) {
      // check if value in range
    }
  }
  
  if (lhs->_type == TRI_AQL_ACCESS_DOUBLE_RANGE) {
  }

  return NULL;
}


static TRI_aql_field_access_t* CreateAccess (TRI_aql_context_t* const context,
                                             const TRI_aql_field_name_t* const field, 
                                             const TRI_aql_node_type_e operator,
                                             const TRI_aql_node_t* const node) {
  TRI_aql_field_access_t* fieldAccess;
  TRI_json_t* value;

  value = TRI_NodeJsonAql(context, node);
  if (!value) {
    return NULL;
  }  

  fieldAccess = (TRI_aql_field_access_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_field_access_t), false);
  if (fieldAccess == NULL) {
    return NULL;
  }

  fieldAccess->_fieldName = TRI_DuplicateString(field->_name._buffer);
  if (fieldAccess->_fieldName == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldAccess);
    return NULL;
  }

  if (operator == AQL_NODE_OPERATOR_BINARY_EQ) {
    fieldAccess->_type = TRI_AQL_ACCESS_EXACT; 
    fieldAccess->_value._value = value;
  } 
  else if (operator == AQL_NODE_OPERATOR_BINARY_LT) { 
    fieldAccess->_type = TRI_AQL_ACCESS_SINGLE_RANGE;
    fieldAccess->_value._singleRange._type = TRI_AQL_RANGE_UPPER_EXCLUDED;
    fieldAccess->_value._singleRange._value = value;
  } 
  else if (operator == AQL_NODE_OPERATOR_BINARY_LE) { 
    fieldAccess->_type = TRI_AQL_ACCESS_SINGLE_RANGE;
    fieldAccess->_value._singleRange._type = TRI_AQL_RANGE_UPPER_INCLUDED;
    fieldAccess->_value._singleRange._value = value;
  } 
  else if (operator == AQL_NODE_OPERATOR_BINARY_GT) { 
    fieldAccess->_type = TRI_AQL_ACCESS_SINGLE_RANGE;
    fieldAccess->_value._singleRange._type = TRI_AQL_RANGE_LOWER_EXCLUDED;
    fieldAccess->_value._singleRange._value = value;
  } 
  else if (operator == AQL_NODE_OPERATOR_BINARY_GE) { 
    fieldAccess->_type = TRI_AQL_ACCESS_SINGLE_RANGE;
    fieldAccess->_value._singleRange._type = TRI_AQL_RANGE_LOWER_INCLUDED;
    fieldAccess->_value._singleRange._value = value;
  }
  else if (operator == AQL_NODE_OPERATOR_BINARY_IN) { 
    fieldAccess->_type = TRI_AQL_ACCESS_LIST;
    fieldAccess->_value._value = value;
    TRI_SortListJson(fieldAccess->_value._value);
  }
  else {
    assert(false);
  }
  
  return fieldAccess;
}

static void CreateFieldAccess (TRI_aql_context_t* const context,
                               const TRI_aql_field_name_t* const field,
                               const TRI_aql_node_type_e operator,
                               const TRI_aql_node_t* const node) {
  TRI_aql_field_access_t* previous;
  TRI_aql_field_access_t* fieldAccess;

  if (!field || !field->_name._buffer) {
    return;
  }
    
  fieldAccess = CreateAccess(context, field, operator, node);
  if (!fieldAccess) { 
    // TODO: mark failure
    return;
  }

  previous = (TRI_aql_field_access_t*) TRI_LookupByKeyAssociativePointer(&context->_ranges, field->_name._buffer);
  if (previous) {
    printf("ENTER MERGE\n");
    TRI_aql_field_access_t* merged = MergeAccess(context, fieldAccess, previous);
    printf("LEAVE MERGE\n");

    // TODO:
    // free previous
    // free fieldAccess
    if (merged) {
      TRI_InsertKeyAssociativePointer(&context->_ranges, merged->_fieldName, merged, true);
    printf("MERGE1\n");
    }

    // previous access exists, must merge
    // merge(previous, new)
    // TRI_InsertKeyAssociativePointer(&context
  }
  else {
    // no previous access exists, no need to merge
    TRI_InsertKeyAssociativePointer(&context->_ranges, fieldAccess->_fieldName, fieldAccess, false);
    printf("INSERT1\n");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_name_t* GetFieldName (TRI_aql_context_t* const context,
                                           const TRI_aql_node_t* const node) {
  if (node->_type == AQL_NODE_ATTRIBUTE_ACCESS) {
    TRI_aql_field_name_t* field = GetFieldName(context, TRI_AQL_NODE_MEMBER(node, 0));

    if (!field) {
      return NULL;
    }

    TRI_AppendCharStringBuffer(&field->_name, '.');
    TRI_AppendStringStringBuffer(&field->_name, TRI_AQL_NODE_STRING(node));
    return field;
  }
  else if (node->_type == AQL_NODE_REFERENCE) {
    TRI_aql_field_name_t* field;
    
    field = (TRI_aql_field_name_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_field_name_t), false);

    if (!field) {
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return NULL;
    }

    field->_variable = TRI_AQL_NODE_STRING(node);
    TRI_InitStringBuffer(&field->_name, TRI_UNKNOWN_MEM_ZONE);
    TRI_AppendStringStringBuffer(&field->_name, TRI_AQL_NODE_STRING(node));

    return field;
  }

  return NULL;
}
////////////////////////////////////////////////////////////////////////////////
/// @brief inspect a filter expression
////////////////////////////////////////////////////////////////////////////////

static void InspectFilter (TRI_aql_context_t* const context,
                           TRI_aql_node_t* node) {
      printf("ITERATION SOMETHING %s\n", TRI_NodeNameAql(node->_type));
  if (node->_type == AQL_NODE_OPERATOR_UNARY_NOT) {
    return;
  }

  if (node->_type == AQL_NODE_OPERATOR_BINARY_OR) {
    TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
    TRI_aql_node_t* rhs = TRI_AQL_NODE_MEMBER(node, 1);

    InspectFilter(context, lhs);
    InspectFilter(context, rhs);
  }

  if (node->_type == AQL_NODE_OPERATOR_BINARY_AND) {
    TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
    TRI_aql_node_t* rhs = TRI_AQL_NODE_MEMBER(node, 1);

    InspectFilter(context, lhs);
    InspectFilter(context, rhs);
  }

  if (node->_type == AQL_NODE_OPERATOR_BINARY_EQ ||
//      node->_type == AQL_NODE_OPERATOR_BINARY_NE ||
      node->_type == AQL_NODE_OPERATOR_BINARY_LT ||
      node->_type == AQL_NODE_OPERATOR_BINARY_LE ||
      node->_type == AQL_NODE_OPERATOR_BINARY_GT ||
      node->_type == AQL_NODE_OPERATOR_BINARY_GE ||
      node->_type == AQL_NODE_OPERATOR_BINARY_IN) {
    TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
    TRI_aql_node_t* rhs = TRI_AQL_NODE_MEMBER(node, 1);

    if (lhs->_type == AQL_NODE_ATTRIBUTE_ACCESS) {
      TRI_aql_field_name_t* field = GetFieldName(context, lhs);

      if (field) {
        CreateFieldAccess(context, field, node->_type, rhs);
        TRI_DestroyStringBuffer(&field->_name);
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, field);
      }
    }
    else if (lhs->_type == AQL_NODE_ATTRIBUTE_ACCESS) {
      TRI_aql_field_name_t* field = GetFieldName(context, rhs);

      if (field) {
        CreateFieldAccess(context, field, node->_type, lhs);
        TRI_DestroyStringBuffer(&field->_name);
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, field);
      }
    }
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief create javascript function code for a relational operation
////////////////////////////////////////////////////////////////////////////////

static TRI_string_buffer_t* RelationCode (const char* const name, 
                                          const TRI_aql_node_t* const lhs,
                                          const TRI_aql_node_t* const rhs) {
  TRI_string_buffer_t* buffer = TRI_CreateStringBuffer(TRI_UNKNOWN_MEM_ZONE);

  if (!lhs || !rhs) {
    return NULL;
  }
  
  if (TRI_AppendStringStringBuffer(buffer, "(function(){return AHUACATL_RELATIONAL_") != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  if (TRI_AppendStringStringBuffer(buffer, name) != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }
  
  if (TRI_AppendStringStringBuffer(buffer, "(") != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }
    
  if (!TRI_NodeJavascriptAql(buffer, lhs)) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }
      
  if (TRI_AppendCharStringBuffer(buffer, ',') != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }
  
  if (!TRI_NodeJavascriptAql(buffer, rhs)) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  if (TRI_AppendStringStringBuffer(buffer, ");})") != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create javascript function code for a function call
////////////////////////////////////////////////////////////////////////////////

static TRI_string_buffer_t* FcallCode (const char* const name, 
                                       const TRI_aql_node_t* const args) {
  TRI_string_buffer_t* buffer = TRI_CreateStringBuffer(TRI_UNKNOWN_MEM_ZONE);
  size_t i;
  size_t n;

  if (!buffer) {
    return NULL;
  }
  
  if (TRI_AppendStringStringBuffer(buffer, "(function(){return AHUACATL_FCALL(") != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }
  
  if (TRI_AppendStringStringBuffer(buffer, name) != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  if (TRI_AppendStringStringBuffer(buffer, ",[") != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  n = args->_members._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* arg = (TRI_aql_node_t*) args->_members._buffer[i];
    if (i > 0) {
      if (TRI_AppendCharStringBuffer(buffer, ',') != TRI_ERROR_NO_ERROR) {
        TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
        return NULL;
      }
    }

    if (!TRI_NodeJavascriptAql(buffer, arg)) {
      TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
      return NULL;
    }
  }

  if (TRI_AppendStringStringBuffer(buffer, "]);})") != TRI_ERROR_NO_ERROR) {
    TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, buffer);
    return NULL;
  }

  return buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a function call
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseFcall (TRI_aql_context_t* const context,
                                      TRI_aql_node_t* node) {
  TRI_aql_node_t* args = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_function_t* function;
  TRI_js_exec_context_t* execContext;
  TRI_string_buffer_t* code;
  TRI_json_t* json;
  size_t i;
  size_t n;

  function = (TRI_aql_function_t*) TRI_AQL_NODE_DATA(node);
  assert(function);

  // check if function is deterministic
  if (!function->_isDeterministic) {
    return node;
  }

  // check if function call arguments are deterministic
  n = args->_members._length;
  for (i = 0; i < n; ++i) {
    TRI_aql_node_t* arg = (TRI_aql_node_t*) args->_members._buffer[i];

    if (!arg || !TRI_IsConstantValueNodeAql(arg)) {
      return node;
    }
  }
 
  // all arguments are constants
  // create the function code
  code = FcallCode(function->_internalName, args);
  if (!code) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }
    
  // execute the function code
  execContext = TRI_CreateExecutionContext(code->_buffer);
  TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, code);

  if (!execContext) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }

  json = TRI_ExecuteResultContext(execContext);
  TRI_FreeExecutionContext(execContext);
  if (!json) {
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_SCRIPT, NULL);
    return NULL;
  }

  // use the constant values instead of the function call node
  node = TRI_JsonNodeAql(context, json);
  if (!node) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  
  LOG_TRACE("optimised function call");

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a sort expression
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseSort (TRI_aql_context_t* const context,
                                     TRI_aql_node_t* node) {
  TRI_aql_node_t* list = TRI_AQL_NODE_MEMBER(node, 0);
  size_t i, n;
    
  if (!list) {
    return node;
  }

  i = 0;
  n = list->_members._length;
  while (i < n) {
    // sort element
    TRI_aql_node_t* element = TRI_AQL_NODE_MEMBER(list, i);
    TRI_aql_node_t* expression = TRI_AQL_NODE_MEMBER(element, 0);

    // check if the sort element is constant
    if (!expression || !TRI_IsConstantValueNodeAql(expression)) {
      ++i;
      continue;
    }

    // sort element is constant so it can be removed
    TRI_RemoveVectorPointer(&list->_members, i);
    --n;

    LOG_TRACE("optimised away sort element");
  }

  if (n == 0) {
    // no members left => sort removed
    LOG_TRACE("optimised away sort");

    return NULL;
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a filter expression
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseFilter (TRI_aql_context_t* const context,
                                       TRI_aql_node_t* node) {
#ifdef RANGE_OPTIMIZER  
  TRI_aql_node_t* expression = TRI_AQL_NODE_MEMBER(node, 0);
  bool result;
   
  if (!expression) {
    return node;
  }
  
  if (!TRI_IsConstantValueNodeAql(expression)) {
    InspectFilter(context, expression);
    return node;
  }

  result = TRI_GetBooleanNodeValueAql(expression);
  if (result) {
    // filter expression is always true => remove it
    LOG_TRACE("optimised away constant filter");

    return NULL;
  }
#endif
  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise an arithmetic operation with one operand
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseUnaryArithmeticOperation (TRI_aql_context_t* const context,
                                                         TRI_aql_node_t* node) {
  TRI_aql_node_t* operand = TRI_AQL_NODE_MEMBER(node, 0);

  if (!operand || !TRI_IsConstantValueNodeAql(operand)) {
    return node;
  }

  if (!TRI_IsNumericValueNodeAql(operand)) {
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE, NULL);
    return node;
  }

  assert(node->_type == AQL_NODE_OPERATOR_UNARY_PLUS ||
         node->_type == AQL_NODE_OPERATOR_UNARY_MINUS);

  if (node->_type == AQL_NODE_OPERATOR_UNARY_PLUS) {
    // + number => number
    node = operand;
  }
  else if (node->_type == AQL_NODE_OPERATOR_UNARY_MINUS) {
    // - number => eval!
    node = TRI_CreateNodeValueDoubleAql(context, - TRI_GetNumericNodeValueAql(operand));
    if (!node) {
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    }
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a boolean operation with one operand
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseUnaryLogicalOperation (TRI_aql_context_t* const context,
                                                      TRI_aql_node_t* node) {
  TRI_aql_node_t* operand = TRI_AQL_NODE_MEMBER(node, 0);

  if (!operand || !TRI_IsConstantValueNodeAql(operand)) {
    return node;
  }

  if (!TRI_IsBooleanValueNodeAql(operand)) {
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE, NULL);
    return node;
  }
  
  assert(node->_type == AQL_NODE_OPERATOR_UNARY_NOT);

  if (node->_type == AQL_NODE_OPERATOR_UNARY_NOT) {
    // ! bool => evaluate
    node = TRI_CreateNodeValueBoolAql(context, ! TRI_GetBooleanNodeValueAql(operand));
    if (!node) {
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    }

    LOG_TRACE("optimised away unary logical operation");
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a boolean operation with two operands
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseBinaryLogicalOperation (TRI_aql_context_t* const context,
                                                       TRI_aql_node_t* node) {
  TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_node_t* rhs = TRI_AQL_NODE_MEMBER(node, 1);
  bool isEligibleLhs;
  bool isEligibleRhs;
  bool lhsValue;

  if (!lhs || !rhs) {
    return node;
  } 

  isEligibleLhs = TRI_IsConstantValueNodeAql(lhs);
  isEligibleRhs = TRI_IsConstantValueNodeAql(rhs);

  if (isEligibleLhs && !TRI_IsBooleanValueNodeAql(lhs)) {
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE, NULL);
    return node;
  }

  if (isEligibleRhs && !TRI_IsBooleanValueNodeAql(rhs)) {
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE, NULL);
    return node;
  }

  if (!isEligibleLhs || !isEligibleRhs) {
    return node;
  }

  lhsValue = TRI_GetBooleanNodeValueAql(lhs);
  
  assert(node->_type == AQL_NODE_OPERATOR_BINARY_AND ||
         node->_type == AQL_NODE_OPERATOR_BINARY_OR);

  LOG_TRACE("optimised away binary logical operation");

  if (node->_type == AQL_NODE_OPERATOR_BINARY_AND) {
    if (lhsValue) {
      // if (true && rhs) => rhs
      return rhs;
    }

    // if (false && rhs) => false
    return lhs;
  } 
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_OR) {
    if (lhsValue) {
      // if (true || rhs) => true
      return lhs;
    }

    // if (false || rhs) => rhs
    return rhs;
  }

  assert(false);

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise a relational operation with two operands
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseBinaryRelationalOperation (TRI_aql_context_t* const context,
                                                          TRI_aql_node_t* node) {
  TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_node_t* rhs = TRI_AQL_NODE_MEMBER(node, 1);
  TRI_js_exec_context_t* execContext;
  TRI_string_buffer_t* code;
  TRI_json_t* json;
  char* func;
  
  if (!lhs || !TRI_IsConstantValueNodeAql(lhs) || !rhs || !TRI_IsConstantValueNodeAql(rhs)) {
    return node;
  } 
  
  if (node->_type == AQL_NODE_OPERATOR_BINARY_EQ) {
    func = "EQUAL";
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_NE) {
    func = "UNEQUAL";
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_GT) {
    func = "GREATER";
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_GE) {
    func = "GREATER_EQUAL";
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_LT) {
    func = "LESS";
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_LE) {
    func = "LESS_EQUAL";
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_IN) {
    func = "IN";
  }
  
  code = RelationCode(func, lhs, rhs); 
  if (!code) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }
    
  // execute the function code
  execContext = TRI_CreateExecutionContext(code->_buffer);
  TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, code);

  if (!execContext) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }

  json = TRI_ExecuteResultContext(execContext);
  TRI_FreeExecutionContext(execContext);
  if (!json) {
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_SCRIPT, NULL);
    return NULL;
  }
  
  // use the constant values instead of the function call node
  node = TRI_JsonNodeAql(context, json);
  if (!node) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
  }
    
  LOG_TRACE("optimised away binary relational operation");

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimise an arithmetic operation with two operands
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* OptimiseBinaryArithmeticOperation (TRI_aql_context_t* const context,
                                                          TRI_aql_node_t* node) {
  TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_node_t* rhs = TRI_AQL_NODE_MEMBER(node, 1);
  bool isEligibleLhs;
  bool isEligibleRhs;
  double value;
  
  if (!lhs || !rhs) {
    return node;
  } 

  isEligibleLhs = TRI_IsConstantValueNodeAql(lhs);
  isEligibleRhs = TRI_IsConstantValueNodeAql(rhs);
  
  if (isEligibleLhs && !TRI_IsNumericValueNodeAql(lhs)) {
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE, NULL);
    return node;
  }

  
  if (isEligibleRhs && !TRI_IsNumericValueNodeAql(rhs)) {
    TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE, NULL);
    return node;
  }
  
  if (!isEligibleLhs || !isEligibleRhs) {
    return node;
  }
  
  assert(node->_type == AQL_NODE_OPERATOR_BINARY_PLUS ||
         node->_type == AQL_NODE_OPERATOR_BINARY_MINUS ||
         node->_type == AQL_NODE_OPERATOR_BINARY_TIMES ||
         node->_type == AQL_NODE_OPERATOR_BINARY_DIV ||
         node->_type == AQL_NODE_OPERATOR_BINARY_MOD);

  if (node->_type == AQL_NODE_OPERATOR_BINARY_PLUS) {
    value = TRI_GetNumericNodeValueAql(lhs) + TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_MINUS) {
    value = TRI_GetNumericNodeValueAql(lhs) - TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_TIMES) {
    value = TRI_GetNumericNodeValueAql(lhs) * TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_DIV) {
    if (TRI_GetNumericNodeValueAql(rhs) == 0.0) {
      TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_DIVISON_BY_ZERO, NULL);
      return node;
    }
    value = TRI_GetNumericNodeValueAql(lhs) / TRI_GetNumericNodeValueAql(rhs);
  }
  else if (node->_type == AQL_NODE_OPERATOR_BINARY_MOD) {
    if (TRI_GetNumericNodeValueAql(rhs) == 0.0) {
      TRI_SetErrorContextAql(context, TRI_ERROR_QUERY_DIVISON_BY_ZERO, NULL);
      return node;
    }
    value = fmod(TRI_GetNumericNodeValueAql(lhs), TRI_GetNumericNodeValueAql(rhs));
  }
  
  node = TRI_CreateNodeValueDoubleAql(context, value);
  if (!node) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }
  
  LOG_TRACE("optimised away binary arithmetic operation");

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief mark a for node
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* MarkFor (TRI_aql_context_t* const context,
                                TRI_aql_node_t* node) {
#ifdef RANGE_OPTIMIZER  
  TRI_aql_node_t* nameNode = TRI_AQL_NODE_MEMBER(node, 0);
  TRI_aql_node_t* expressionNode = TRI_AQL_NODE_MEMBER(node, 1);

  if (!nameNode || !expressionNode) {
    return node;
  }

  if (expressionNode->_type == AQL_NODE_COLLECTION) {
    char* varName = TRI_AQL_NODE_STRING(nameNode);
    char* collectionName = TRI_AQL_NODE_STRING(expressionNode);

    if (!varName || !collectionName) {
      return node;
    }
  }
#endif

  return node;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief fold constants in a node
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_node_t* ModifyNode (void* data, TRI_aql_node_t* node) {
  TRI_aql_context_t* context = (TRI_aql_context_t*) data;

  if (!node) {
    return NULL;
  }

  switch (node->_type) {
    case AQL_NODE_OPERATOR_UNARY_PLUS:
    case AQL_NODE_OPERATOR_UNARY_MINUS:
      return OptimiseUnaryArithmeticOperation(context, node);
    case AQL_NODE_OPERATOR_UNARY_NOT:
      return OptimiseUnaryLogicalOperation(context, node);
    case AQL_NODE_OPERATOR_BINARY_AND:
    case AQL_NODE_OPERATOR_BINARY_OR:
      return OptimiseBinaryLogicalOperation(context, node);
    case AQL_NODE_OPERATOR_BINARY_EQ:
    case AQL_NODE_OPERATOR_BINARY_NE:
    case AQL_NODE_OPERATOR_BINARY_LT:
    case AQL_NODE_OPERATOR_BINARY_LE:
    case AQL_NODE_OPERATOR_BINARY_GT:
    case AQL_NODE_OPERATOR_BINARY_GE:
    case AQL_NODE_OPERATOR_BINARY_IN:
      return OptimiseBinaryRelationalOperation(context, node);
    case AQL_NODE_OPERATOR_BINARY_PLUS:
    case AQL_NODE_OPERATOR_BINARY_MINUS:
    case AQL_NODE_OPERATOR_BINARY_TIMES:
    case AQL_NODE_OPERATOR_BINARY_DIV:
    case AQL_NODE_OPERATOR_BINARY_MOD:
      return OptimiseBinaryArithmeticOperation(context, node);
    case AQL_NODE_SORT:
      return OptimiseSort(context, node);
    case AQL_NODE_FILTER:
      return OptimiseFilter(context, node);
    case AQL_NODE_FCALL:
      return OptimiseFcall(context, node);
    case AQL_NODE_FOR:
      return MarkFor(context, node);
    default: 
      break;
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @} 
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief fold constants recursively
////////////////////////////////////////////////////////////////////////////////

TRI_aql_node_t* TRI_FoldConstantsAql (TRI_aql_context_t* const context,
                                      TRI_aql_node_t* node) {
  TRI_aql_modify_tree_walker_t* walker;
  
  // todo: must free
  TRI_InitAssociativePointer(&context->_ranges,
                             TRI_UNKNOWN_MEM_ZONE, 
                             &TRI_HashStringKeyAssociativePointer, 
                             &HashFieldAccess,
                             &EqualFieldAccess,
                             NULL);

  walker = TRI_CreateModifyTreeWalkerAql((void*) context, &ModifyNode);
  if (!walker) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return node;
  }

  node = TRI_ModifyWalkTreeAql(walker, node); 

  TRI_FreeModifyTreeWalkerAql(walker);

#ifdef RANGE_OPTIMIZER
  size_t i;  
  for (i = 0; i < context->_ranges._nrAlloc; ++i) {
    TRI_aql_field_access_t* fieldAccess = context->_ranges._table[i]; 

    if (!fieldAccess) {
      continue;
    }

    printf("\nFIELD ACCESS\n- FIELD: %s\n",fieldAccess->_fieldName);
    printf("- TYPE: %s\n", AccessName(fieldAccess->_type));
    if (fieldAccess->_type == TRI_AQL_ACCESS_EXACT || fieldAccess->_type == TRI_AQL_ACCESS_LIST) {
      TRI_string_buffer_t b;
      TRI_InitStringBuffer(&b, TRI_UNKNOWN_MEM_ZONE);
      TRI_StringifyJson(&b, fieldAccess->_value._value);

      printf("- VALUE: %s\n", b._buffer);
    }
    else if (fieldAccess->_type == TRI_AQL_ACCESS_SINGLE_RANGE) {
      TRI_string_buffer_t b;
      TRI_InitStringBuffer(&b, TRI_UNKNOWN_MEM_ZONE);
      TRI_StringifyJson(&b, fieldAccess->_value._singleRange._value);

      printf("- VALUE: %s\n", b._buffer);
    }
    else if (fieldAccess->_type == TRI_AQL_ACCESS_DOUBLE_RANGE) {
      TRI_string_buffer_t b;
      TRI_InitStringBuffer(&b, TRI_UNKNOWN_MEM_ZONE);
      TRI_StringifyJson(&b, fieldAccess->_value._between._lower._value);
      TRI_AppendStringStringBuffer(&b, ", ");
      TRI_StringifyJson(&b, fieldAccess->_value._between._upper._value);

      printf("- VALUE: %s\n", b._buffer);
    }
  }
#endif

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

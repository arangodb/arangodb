////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, access optimiser
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

#include "Ahuacatl/ahuacatl-access-optimiser.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_DEBUG_AQL
////////////////////////////////////////////////////////////////////////////////
/// @brief return the name of an access type
////////////////////////////////////////////////////////////////////////////////

static char* AccessName (const TRI_aql_access_e type) {
  switch (type) {
    case TRI_AQL_ACCESS_ALL: 
      return "all";
    case TRI_AQL_ACCESS_REFERENCE: 
      return "reference";
    case TRI_AQL_ACCESS_IMPOSSIBLE: 
      return "impossible";
    case TRI_AQL_ACCESS_EXACT: 
      return "exact";
    case TRI_AQL_ACCESS_LIST: 
      return "list";
    case TRI_AQL_ACCESS_RANGE_SINGLE: 
      return "single range";
    case TRI_AQL_ACCESS_RANGE_DOUBLE: 
      return "double range";
  }

  assert(false);
  return NULL;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief set the length of the variable name for a field access struct
////////////////////////////////////////////////////////////////////////////////

static void SetNameLength (TRI_aql_field_access_t* const fieldAccess) {
  char* dotPosition;

  assert(fieldAccess);
  assert(fieldAccess->_fullName);
  
  dotPosition = strchr(fieldAccess->_fullName, '.');
  if (dotPosition == NULL) {
    // field does not contain .
    fieldAccess->_variableNameLength = strlen(fieldAccess->_fullName);
  }
  else {
    fieldAccess->_variableNameLength = dotPosition - fieldAccess->_fullName;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free access member data, but do not free the access struct itself
////////////////////////////////////////////////////////////////////////////////

static void FreeAccessMembers (TRI_aql_field_access_t* const fieldAccess) {
  assert(fieldAccess);

  switch (fieldAccess->_type) {
    case TRI_AQL_ACCESS_EXACT:
    case TRI_AQL_ACCESS_LIST: {
      if (fieldAccess->_value._value) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_value._value);
      }
      break;
    }

    case TRI_AQL_ACCESS_RANGE_SINGLE: {
      if (fieldAccess->_value._singleRange._value) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_value._singleRange._value);
      }
      break;
    }

    case TRI_AQL_ACCESS_RANGE_DOUBLE: {
      if (fieldAccess->_value._between._lower._value) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_value._between._lower._value);
      }
      if (fieldAccess->_value._between._upper._value) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_value._between._upper._value);
      }
      break;
    }
    
    case TRI_AQL_ACCESS_REFERENCE: {
      if (fieldAccess->_value._value) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_value._value);
      }
      break;
    }

    case TRI_AQL_ACCESS_ALL:
    case TRI_AQL_ACCESS_IMPOSSIBLE: {
      // nada
      break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a field access structure of the given type
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* CreateFieldAccess (TRI_aql_context_t* const context,
                                                  const TRI_aql_access_e type,
                                                  const char* const fullName) {
  TRI_aql_field_access_t* fieldAccess;
 
  fieldAccess = (TRI_aql_field_access_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_field_access_t), false);
  if (fieldAccess == NULL) {
    // OOM
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }

  fieldAccess->_fullName = TRI_DuplicateString(fullName);
  if (fieldAccess->_fullName == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldAccess);

    return NULL;
  }

  SetNameLength(fieldAccess);

  fieldAccess->_type = type;

  return fieldAccess;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                  attribute access merge functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two access structures using a logical AND
///
/// left hand operand is an impossible range, so the result is also the
/// impossible range
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeAndImpossible (TRI_aql_context_t* const context,
                                                   TRI_aql_field_access_t* lhs,
                                                   TRI_aql_field_access_t* rhs) {
  // impossible merged with anything just returns impossible
  assert(lhs->_type == TRI_AQL_ACCESS_IMPOSSIBLE);
  TRI_FreeAccessAql(rhs);

  return lhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two access structures using a logical AND
///
/// left hand operand is all items, so the result is the other operand
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeAndAll (TRI_aql_context_t* const context,
                                            TRI_aql_field_access_t* lhs,
                                            TRI_aql_field_access_t* rhs) {
  // all merged with anything just returns the other side
  assert(lhs->_type == TRI_AQL_ACCESS_ALL);
  TRI_FreeAccessAql(lhs);

  return rhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two access structures using a logical AND
///
/// left hand operand is a reference, so it will always be returned
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeAndReference (TRI_aql_context_t* const context,
                                                  TRI_aql_field_access_t* lhs,
                                                  TRI_aql_field_access_t* rhs) {
  assert(lhs->_type == TRI_AQL_ACCESS_REFERENCE);
  
  // reference always wins
  TRI_FreeAccessAql(rhs);

  return lhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two access structures using a logical AND
///
/// left hand operand is the exact match, the result type depends on the right
/// hand operand type
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeAndExact (TRI_aql_context_t* const context,
                                              TRI_aql_field_access_t* lhs,
                                              TRI_aql_field_access_t* rhs) {
  assert(lhs->_type == TRI_AQL_ACCESS_EXACT);

  if (rhs->_type == TRI_AQL_ACCESS_EXACT) {
    // check if values are identical
    bool isSame = TRI_CheckSameValueJson(lhs->_value._value, rhs->_value._value);

    TRI_FreeAccessAql(rhs);

    if (!isSame) {
      // lhs and rhs values are non-identical, return impossible
      FreeAccessMembers(lhs);
      lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;
    }

    return lhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_LIST) {
    // check if lhs is contained in rhs list
    bool inList = TRI_CheckInListJson(lhs->_value._value, rhs->_value._value);

    TRI_FreeAccessAql(rhs);

    if (!inList) {
      // lhs value is not in rhs list, return impossible
      FreeAccessMembers(lhs);
      lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;
    }

    return lhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_RANGE_SINGLE) {
    // check if value is in range
    int result = TRI_CompareValuesJson(lhs->_value._value, rhs->_value._singleRange._value);

    bool contained = ((rhs->_value._singleRange._type == TRI_AQL_RANGE_LOWER_EXCLUDED && result > 0) ||
        (rhs->_value._singleRange._type == TRI_AQL_RANGE_LOWER_INCLUDED && result >= 0) ||
        (rhs->_value._singleRange._type == TRI_AQL_RANGE_UPPER_EXCLUDED && result < 0) ||
        (rhs->_value._singleRange._type == TRI_AQL_RANGE_UPPER_INCLUDED && result <= 0));

    if (!contained) {
      // lhs value is not contained in rhs range, return impossible
      TRI_FreeAccessAql(rhs);
      FreeAccessMembers(lhs);
      lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

      return lhs;
    }

    // lhs value is contained in rhs range, simply return rhs
    TRI_FreeAccessAql(lhs);

    return rhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_RANGE_DOUBLE) {
    // check if value is in range
    int result;
    bool contained;

    // compare lower end
    result = TRI_CompareValuesJson(lhs->_value._value, rhs->_value._between._lower._value);

    contained = ((rhs->_value._between._lower._type == TRI_AQL_RANGE_LOWER_EXCLUDED && result > 0) ||
        (rhs->_value._between._lower._type == TRI_AQL_RANGE_LOWER_INCLUDED && result >= 0));

    if (!contained) {
      // lhs value is not contained in rhs range, return impossible
      TRI_FreeAccessAql(rhs);
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
      TRI_FreeAccessAql(rhs);
      FreeAccessMembers(lhs);
      lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

      return lhs;
    }

    // lhs value is contained in rhs range, return rhs
    TRI_FreeAccessAql(lhs);

    return rhs;
  }

  assert(false);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two access structures using a logical AND
///
/// left hand operand is a value list, the result type depends on the right
/// hand operand type
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeAndList (TRI_aql_context_t* const context,
                                             TRI_aql_field_access_t* lhs,
                                             TRI_aql_field_access_t* rhs) {
  assert(lhs->_type == TRI_AQL_ACCESS_LIST);

  if (rhs->_type == TRI_AQL_ACCESS_LIST) {
    // make a list of both
    TRI_json_t* merged = TRI_IntersectListsJson(lhs->_value._value, rhs->_value._value, true);
    if (!merged) {
      // OOM
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return lhs;
    }

    FreeAccessMembers(lhs);
    TRI_FreeAccessAql(rhs);

    if (merged->_value._objects._length > 0) {
      // merged list is not empty
      lhs->_value._value = merged;
    }
    else {
      // merged list is empty
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, merged);
      lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;
    }

    return lhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_RANGE_SINGLE) {
    TRI_json_t* listInRange;

    if (rhs->_value._singleRange._type == TRI_AQL_RANGE_LOWER_INCLUDED ||
        rhs->_value._singleRange._type == TRI_AQL_RANGE_LOWER_EXCLUDED) {
      listInRange = TRI_BetweenListJson(lhs->_value._singleRange._value, 
          rhs->_value._singleRange._value, 
          rhs->_value._singleRange._type == TRI_AQL_RANGE_LOWER_INCLUDED, 
          NULL, 
          true);
    }
    else {
      listInRange = TRI_BetweenListJson(lhs->_value._singleRange._value,
          NULL,
          true, 
          rhs->_value._singleRange._value, 
          rhs->_value._singleRange._type == TRI_AQL_RANGE_UPPER_INCLUDED);
    }

    if (!listInRange) {
      // OOM
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      FreeAccessMembers(rhs);
      return lhs;
    }

    FreeAccessMembers(lhs);
    TRI_FreeAccessAql(rhs);

    if (listInRange->_value._objects._length > 0) {
      // merged list is not empty
      lhs->_value._value = listInRange;
    }
    else {
      // merged list is empty
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, listInRange);
      lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;
    }

    return lhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_RANGE_DOUBLE) {
    // check if value in range
    TRI_json_t* listInRange;

    listInRange = TRI_BetweenListJson(lhs->_value._singleRange._value,
        rhs->_value._between._lower._value,
        rhs->_value._between._lower._type == TRI_AQL_RANGE_LOWER_INCLUDED, 
        rhs->_value._between._upper._value, 
        rhs->_value._between._upper._type == TRI_AQL_RANGE_UPPER_INCLUDED);

    if (!listInRange) {
      // OOM
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      FreeAccessMembers(rhs);
      return lhs;
    }

    FreeAccessMembers(lhs);
    TRI_FreeAccessAql(rhs);

    if (listInRange->_value._objects._length > 0) {
      // merged list is not empty
      lhs->_value._value = listInRange;
    }
    else {
      // merged list is empty
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, listInRange);
      lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;
    }

    return lhs;
  }
  
  assert(false);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two access structures using a logical AND
///
/// left hand operand is a single range, the result type depends on the right
/// hand operand type
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeAndRangeSingle (TRI_aql_context_t* const context,
                                                    TRI_aql_field_access_t* lhs,
                                                    TRI_aql_field_access_t* rhs) {
  assert(lhs->_type == TRI_AQL_ACCESS_RANGE_SINGLE);

  if (rhs->_type == TRI_AQL_ACCESS_RANGE_SINGLE) {
    TRI_json_t* lhsValue;
    TRI_json_t* rhsValue;
    TRI_aql_range_e lhsType;
    TRI_aql_range_e rhsType;
    int compareResult;

    if (lhs->_value._singleRange._type > rhs->_value._singleRange._type) {
      // swap operands so they are always sorted
      TRI_aql_field_access_t* tmp = lhs;
      lhs = rhs;
      rhs = tmp;
    }

    compareResult = TRI_CompareValuesJson(lhs->_value._singleRange._value, rhs->_value._singleRange._value);
    lhsType = lhs->_value._singleRange._type;
    rhsType = rhs->_value._singleRange._type;
    lhsValue = lhs->_value._singleRange._value;
    rhsValue = rhs->_value._singleRange._value;

    // check if ranges overlap
    if ((lhsType == TRI_AQL_RANGE_LOWER_EXCLUDED && rhsType == TRI_AQL_RANGE_LOWER_EXCLUDED) ||
        (lhsType == TRI_AQL_RANGE_LOWER_INCLUDED && rhsType == TRI_AQL_RANGE_LOWER_INCLUDED)) {
      // > && >
      // >= && >=
      if (compareResult > 0) {
        // lhs > rhs
        TRI_FreeAccessAql(rhs);

        return lhs;
      }
      else {
        TRI_FreeAccessAql(lhs);

        return rhs;
      }
    }
    else if ((lhsType == TRI_AQL_RANGE_UPPER_EXCLUDED && rhsType == TRI_AQL_RANGE_UPPER_EXCLUDED) ||
        (lhsType == TRI_AQL_RANGE_UPPER_INCLUDED && rhsType == TRI_AQL_RANGE_UPPER_INCLUDED)) {
      // < && <
      // <= && <=
      if (compareResult > 0) {
        // lhs > rhs
        TRI_FreeAccessAql(lhs);

        return rhs;
      }
      else {
        TRI_FreeAccessAql(rhs);

        return lhs;
      }
    }
    else if (lhsType == TRI_AQL_RANGE_LOWER_EXCLUDED && rhsType == TRI_AQL_RANGE_LOWER_INCLUDED) {
      // > && >=
      if (compareResult >= 0) {
        // lhs > rhs
        TRI_FreeAccessAql(rhs);

        return lhs;
      } 
      else {
        TRI_FreeAccessAql(lhs);

        return rhs;
      }
    }
    else if (lhsType == TRI_AQL_RANGE_LOWER_EXCLUDED && rhsType == TRI_AQL_RANGE_UPPER_EXCLUDED) {
      // > && <
      if (compareResult < 0) {
        // save pointers
        lhsValue = lhs->_value._singleRange._value;
        rhsValue = rhs->_value._singleRange._value;
        rhs->_value._singleRange._value = NULL;
        TRI_FreeAccessAql(rhs); 

        lhs->_type = TRI_AQL_ACCESS_RANGE_DOUBLE;
        lhs->_value._between._lower._type = lhsType;
        lhs->_value._between._lower._value = lhsValue;
        lhs->_value._between._upper._type = rhsType;
        lhs->_value._between._upper._value = rhsValue;

        return lhs;
      }
      else {
        TRI_FreeAccessAql(rhs);
        FreeAccessMembers(lhs);
        lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

        return lhs;
      }
    }
    else if ((lhsType == TRI_AQL_RANGE_LOWER_EXCLUDED && rhsType == TRI_AQL_RANGE_UPPER_INCLUDED) ||
        (lhsType == TRI_AQL_RANGE_LOWER_INCLUDED && rhsType == TRI_AQL_RANGE_UPPER_EXCLUDED)) {
      // > && <=
      // >= && <
      if (compareResult < 0) {
        // save pointers
        lhsValue = lhs->_value._singleRange._value;
        rhsValue = rhs->_value._singleRange._value;
        rhs->_value._singleRange._value = NULL;
        TRI_FreeAccessAql(rhs); 

        lhs->_type = TRI_AQL_ACCESS_RANGE_DOUBLE;
        lhs->_value._between._lower._type = lhsType;
        lhs->_value._between._lower._value = lhsValue;
        lhs->_value._between._upper._type = rhsType;
        lhs->_value._between._upper._value = rhsValue;

        return lhs;
      }
      else {
        TRI_FreeAccessAql(rhs);
        FreeAccessMembers(lhs);
        lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

        return lhs;
      }
    }
    else if (lhsType == TRI_AQL_RANGE_LOWER_INCLUDED && rhsType == TRI_AQL_RANGE_UPPER_INCLUDED) {
      // >= && <=
      if (compareResult < 0) {
        // save pointers
        lhsValue = lhs->_value._singleRange._value;
        rhsValue = rhs->_value._singleRange._value;
        rhs->_value._singleRange._value = NULL;
        TRI_FreeAccessAql(rhs); 

        lhs->_type = TRI_AQL_ACCESS_RANGE_DOUBLE;
        lhs->_value._between._lower._type = lhsType;
        lhs->_value._between._lower._value = lhsValue;
        lhs->_value._between._upper._type = rhsType;
        lhs->_value._between._upper._value = rhsValue;

        return lhs;
      }
      else if (compareResult == 0) {
        TRI_FreeAccessAql(rhs);

        // save pointer
        lhsValue = lhs->_value._singleRange._value;
        lhs->_type = TRI_AQL_ACCESS_EXACT;
        lhs->_value._value = lhsValue;
        return lhs;
      }
      else {
        TRI_FreeAccessAql(rhs);
        FreeAccessMembers(lhs);
        lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

        return lhs;
      }
    }
    else if (lhsType == TRI_AQL_RANGE_UPPER_EXCLUDED && rhsType == TRI_AQL_RANGE_UPPER_INCLUDED) {
      // < && <=
      if (compareResult <= 0) {
        TRI_FreeAccessAql(rhs);

        return lhs;
      }
      else {
        TRI_FreeAccessAql(lhs);

        return rhs;
      }
    }
  }

  if (rhs->_type == TRI_AQL_ACCESS_RANGE_DOUBLE) {
    int compareResult;

    if (lhs->_value._singleRange._type == TRI_AQL_RANGE_LOWER_EXCLUDED) {
      compareResult = TRI_CompareValuesJson(lhs->_value._singleRange._value, rhs->_value._between._upper._value);
      if (compareResult >= 0) {
        // lhs value is bigger than rhs upper bound
        TRI_FreeAccessAql(rhs);
        FreeAccessMembers(lhs);
        lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

        return lhs;
      }
      
      compareResult = TRI_CompareValuesJson(lhs->_value._singleRange._value, rhs->_value._between._lower._value);
      if (compareResult > 0) {
        // lhs value is bigger than rhs lower bound
        rhs->_value._between._lower._type = lhs->_value._singleRange._type;
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, rhs->_value._between._lower._value);
        rhs->_value._between._lower._value = lhs->_value._singleRange._value;
        lhs->_value._singleRange._value = NULL;
      }
      else if (compareResult == 0) {
        // lhs value is equal to rhs lower bound
        if (rhs->_value._between._lower._type == TRI_AQL_RANGE_LOWER_INCLUDED) {
          rhs->_value._between._lower._type = TRI_AQL_RANGE_LOWER_EXCLUDED;
        }
      }
      // else intentionally left out

      TRI_FreeAccessAql(lhs);

      return rhs;
    }
    
    if (lhs->_value._singleRange._type == TRI_AQL_RANGE_LOWER_INCLUDED) {
      compareResult = TRI_CompareValuesJson(lhs->_value._singleRange._value, rhs->_value._between._upper._value);
      if (compareResult > 0 || (compareResult == 0 && rhs->_value._between._upper._type == TRI_AQL_RANGE_UPPER_EXCLUDED)) {
        // lhs value is bigger than rhs upper bound
        TRI_FreeAccessAql(rhs);
        FreeAccessMembers(lhs);

        lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

        return lhs;
      }
      else if (compareResult == 0) {
        // save pointer
        TRI_json_t* value = lhs->_value._singleRange._value;
        
        TRI_FreeAccessAql(rhs);
        lhs->_value._singleRange._value = NULL;
        FreeAccessMembers(lhs);

        lhs->_value._singleRange._type = TRI_AQL_ACCESS_EXACT;
        lhs->_value._singleRange._value = value;

        return lhs;
      }
      
      compareResult = TRI_CompareValuesJson(lhs->_value._singleRange._value, rhs->_value._between._lower._value);
      if (compareResult > 0) {
        // lhs value is bigger than rhs lower bound
        rhs->_value._between._lower._type = lhs->_value._singleRange._type;
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, rhs->_value._between._lower._value);
        rhs->_value._between._lower._value = lhs->_value._singleRange._value;
        lhs->_value._singleRange._value = NULL;
      }
      // else intentionally left out

      TRI_FreeAccessAql(lhs);

      return rhs;
    }
    
    if (lhs->_value._singleRange._type == TRI_AQL_RANGE_UPPER_EXCLUDED) {
      compareResult = TRI_CompareValuesJson(lhs->_value._singleRange._value, rhs->_value._between._lower._value);
      if (compareResult <= 0) {
        // lhs value is smaller than rhs lower bound
        TRI_FreeAccessAql(rhs);
        FreeAccessMembers(lhs);
        lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

        return lhs;
      }
      
      compareResult = TRI_CompareValuesJson(lhs->_value._singleRange._value, rhs->_value._between._upper._value);
      if (compareResult < 0) {
        // lhs value is smaller than rhs upper bound
        rhs->_value._between._upper._type = lhs->_value._singleRange._type;
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, rhs->_value._between._upper._value);
        rhs->_value._between._upper._value = lhs->_value._singleRange._value;
        lhs->_value._singleRange._value = NULL;
      }
      else if (compareResult == 0) {
        // lhs value is equal to rhs lower bound
        if (rhs->_value._between._upper._type == TRI_AQL_RANGE_UPPER_INCLUDED) {
          rhs->_value._between._upper._type = TRI_AQL_RANGE_UPPER_EXCLUDED;
        }
      }
      // else intentionally left out

      TRI_FreeAccessAql(lhs);

      return rhs;
    }
    
    if (lhs->_value._singleRange._type == TRI_AQL_RANGE_UPPER_INCLUDED) {
      compareResult = TRI_CompareValuesJson(lhs->_value._singleRange._value, rhs->_value._between._lower._value);
      if (compareResult < 0 || (compareResult == 0 && rhs->_value._between._lower._type == TRI_AQL_RANGE_LOWER_EXCLUDED)) {
        // lhs value is smaller than rhs lower bound
        TRI_FreeAccessAql(rhs);
        FreeAccessMembers(lhs);
        lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

        return lhs;
      }
      else if (compareResult == 0) {
        // save pointer
        TRI_json_t* value = lhs->_value._singleRange._value;
        
        TRI_FreeAccessAql(rhs);
        lhs->_value._singleRange._value = NULL;
        FreeAccessMembers(lhs);

        lhs->_value._singleRange._type = TRI_AQL_ACCESS_EXACT;
        lhs->_value._singleRange._value = value;

        return lhs;
      }
      
      compareResult = TRI_CompareValuesJson(lhs->_value._singleRange._value, rhs->_value._between._upper._value);
      if (compareResult < 0) {
        // lhs value is smaller than rhs upper bound
        rhs->_value._between._upper._type = lhs->_value._singleRange._type;
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, rhs->_value._between._upper._value);
        rhs->_value._between._upper._value = lhs->_value._singleRange._value;
        lhs->_value._singleRange._value = NULL;
      }
      // else intentionally left out

      TRI_FreeAccessAql(lhs);

      return rhs;
    }

    return lhs;
  }

  assert(false);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two access structures using a logical AND
///
/// left hand operand is a double range, the result type depends on the right
/// hand operand type
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeAndRangeDouble (TRI_aql_context_t* const context,
                                                    TRI_aql_field_access_t* lhs,
                                                    TRI_aql_field_access_t* rhs) {
  assert(false);
  /* this should never be called. let's see if this assumption is true or not */
  assert(lhs->_type == TRI_AQL_ACCESS_RANGE_DOUBLE);

  return lhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two access structures using a logical OR
///
/// left hand operand is an impossible range, so the result is the right hand 
/// operator
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeOrImpossible (TRI_aql_context_t* const context,
                                                  TRI_aql_field_access_t* lhs,
                                                  TRI_aql_field_access_t* rhs) {
  // impossible merged with anything just returns the other side
  assert(lhs->_type == TRI_AQL_ACCESS_IMPOSSIBLE);
  TRI_FreeAccessAql(lhs);

  return rhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two access structures using a logical OR
///
/// left hand operand is all items, so the result is also all
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeOrAll (TRI_aql_context_t* const context,
                                           TRI_aql_field_access_t* lhs,
                                           TRI_aql_field_access_t* rhs) {
  // all merged with anything just returns all
  assert(lhs->_type == TRI_AQL_ACCESS_ALL);
  TRI_FreeAccessAql(rhs);

  return lhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two access structures using a logical OR
///
/// left hand operand is reference access
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeOrReference (TRI_aql_context_t* const context,
                                                 TRI_aql_field_access_t* lhs,
                                                 TRI_aql_field_access_t* rhs) {
  assert(lhs->_type == TRI_AQL_ACCESS_REFERENCE);

  if (rhs->_type == TRI_AQL_ACCESS_REFERENCE) {
    // if rhs is also a reference, we can keep it if both refer to the same value
    if (TRI_EqualString(lhs->_value._value->_value._string.data, rhs->_value._value->_value._string.data)) {
      TRI_FreeAccessAql(rhs);

      return lhs;
    }
  }

  // for everything else, we have to use the ALL range unfortunately

  TRI_FreeAccessAql(rhs);
  FreeAccessMembers(lhs);
  lhs->_type = TRI_AQL_ACCESS_ALL;

  return lhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two access structures using a logical OR
///
/// left hand operand is the exact match, the result type depends on the right
/// hand operand type
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeOrExact (TRI_aql_context_t* const context,
                                             TRI_aql_field_access_t* lhs,
                                             TRI_aql_field_access_t* rhs) {
  assert(lhs->_type == TRI_AQL_ACCESS_EXACT);

  if (rhs->_type == TRI_AQL_ACCESS_EXACT) {
    TRI_json_t* result;

    // check if values are identical
    if ( TRI_CheckSameValueJson(lhs->_value._value, rhs->_value._value)) {
      // lhs and rhs values are identical, return lhs
      TRI_FreeAccessAql(rhs);
    
      return lhs;
    }

    // make a list with both values
    result = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
    if (!result) {
      // OOM
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      TRI_FreeAccessAql(rhs);

      return lhs;
    }

    TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, lhs->_value._value);
    TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, rhs->_value._value);
    TRI_SortListJson(result);

    TRI_FreeAccessAql(lhs);
    FreeAccessMembers(rhs);

    rhs->_type = TRI_AQL_ACCESS_LIST;
    rhs->_value._value = result;

    return rhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_LIST) {
    // check if lhs is contained in rhs list
    if (TRI_CheckInListJson(lhs->_value._value, rhs->_value._value)) {
      // lhs is contained in rhs, we can return rhs
      TRI_FreeAccessAql(lhs);

      return rhs;
    }

    // lhs is not contained, we need to add it to the list
    TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, rhs->_value._value, lhs->_value._value);
    TRI_SortListJson(rhs->_value._value);

    TRI_FreeAccessAql(lhs);

    return rhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_RANGE_SINGLE) {
    // check if value is in range
    int result = TRI_CompareValuesJson(lhs->_value._value, rhs->_value._singleRange._value);

    bool contained = ((rhs->_value._singleRange._type == TRI_AQL_RANGE_LOWER_EXCLUDED && result > 0) ||
        (rhs->_value._singleRange._type == TRI_AQL_RANGE_LOWER_INCLUDED && result >= 0) ||
        (rhs->_value._singleRange._type == TRI_AQL_RANGE_UPPER_EXCLUDED && result < 0) ||
        (rhs->_value._singleRange._type == TRI_AQL_RANGE_UPPER_INCLUDED && result <= 0));

    if (!contained) {
      // lhs value is not contained in rhs range, return all
      TRI_FreeAccessAql(rhs);
      FreeAccessMembers(lhs);
      lhs->_type = TRI_AQL_ACCESS_ALL;

      return lhs;
    }

    // lhs value is contained in rhs range, simply return rhs
    TRI_FreeAccessAql(lhs);

    return rhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_RANGE_DOUBLE) {
    // check if value is in range
    int result;
    bool contained;

    // compare lower end
    result = TRI_CompareValuesJson(lhs->_value._value, rhs->_value._between._lower._value);

    contained = ((rhs->_value._between._lower._type == TRI_AQL_RANGE_LOWER_EXCLUDED && result > 0) ||
        (rhs->_value._between._lower._type == TRI_AQL_RANGE_LOWER_INCLUDED && result >= 0));

    if (!contained) {
      // lhs value is not contained in rhs range, return all
      TRI_FreeAccessAql(rhs);
      FreeAccessMembers(lhs);
      lhs->_type = TRI_AQL_ACCESS_ALL;

      return lhs;
    }

    // compare upper end     
    result = TRI_CompareValuesJson(lhs->_value._value, rhs->_value._between._upper._value);

    contained = ((rhs->_value._between._upper._type == TRI_AQL_RANGE_UPPER_EXCLUDED && result < 0) ||
        (rhs->_value._between._upper._type == TRI_AQL_RANGE_UPPER_INCLUDED && result <= 0));

    if (!contained) {
      // lhs value is not contained in rhs range, return all
      TRI_FreeAccessAql(rhs);
      FreeAccessMembers(lhs);
      lhs->_type = TRI_AQL_ACCESS_ALL;

      return lhs;
    }

    // lhs value is contained in rhs range, return rhs
    TRI_FreeAccessAql(lhs);

    return rhs;
  }

  assert(false);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two access structures using a logical OR 
///
/// left hand operand is a value list, the result type depends on the right
/// hand operand type
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeOrList (TRI_aql_context_t* const context,
                                            TRI_aql_field_access_t* lhs,
                                            TRI_aql_field_access_t* rhs) {
  assert(lhs->_type == TRI_AQL_ACCESS_LIST);

  if (rhs->_type == TRI_AQL_ACCESS_LIST) {
    // make a list of both
    TRI_json_t* merged = TRI_UnionizeListsJson(lhs->_value._value, rhs->_value._value, true);
    if (!merged) {
      // OOM
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return lhs;
    }

    FreeAccessMembers(lhs);
    TRI_FreeAccessAql(rhs);

    if (merged->_value._objects._length > 0) {
      // merged list is not empty
      lhs->_value._value = merged;
    }
    else {
      // merged list is empty
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, merged);
      lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;
    }

    return lhs;
  }
      
  // for all other combinations, we give up
  TRI_FreeAccessAql(rhs);
  FreeAccessMembers(lhs);

  lhs->_type = TRI_AQL_ACCESS_ALL;

  return lhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two access structures using a logical OR
///
/// left hand operand is a single range, the result type depends on the right
/// hand operand type
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeOrRangeSingle (TRI_aql_context_t* const context,
                                                   TRI_aql_field_access_t* lhs,
                                                   TRI_aql_field_access_t* rhs) {
  assert(lhs->_type == TRI_AQL_ACCESS_RANGE_SINGLE);

  if (rhs->_type == TRI_AQL_ACCESS_RANGE_SINGLE) {
    TRI_aql_range_e lhsType;
    TRI_aql_range_e rhsType;
    int compareResult;

    if (lhs->_value._singleRange._type > rhs->_value._singleRange._type) {
      // swap operands so they are always sorted
      TRI_aql_field_access_t* tmp = lhs;
      lhs = rhs;
      rhs = tmp;
    }

    compareResult = TRI_CompareValuesJson(lhs->_value._singleRange._value, rhs->_value._singleRange._value);
    lhsType = lhs->_value._singleRange._type;
    rhsType = rhs->_value._singleRange._type;

    // check if ranges overlap
    if ((lhsType == TRI_AQL_RANGE_LOWER_EXCLUDED && rhsType == TRI_AQL_RANGE_LOWER_EXCLUDED) ||
        (lhsType == TRI_AQL_RANGE_LOWER_INCLUDED && rhsType == TRI_AQL_RANGE_LOWER_INCLUDED)) {
      // > && >
      // >= && >=
      if (compareResult > 0) {
        // lhs > rhs
        TRI_FreeAccessAql(lhs);

        return rhs;
      }
      else {
        TRI_FreeAccessAql(rhs);

        return lhs;
      }
    }
    else if ((lhsType == TRI_AQL_RANGE_UPPER_EXCLUDED && rhsType == TRI_AQL_RANGE_UPPER_EXCLUDED) ||
        (lhsType == TRI_AQL_RANGE_UPPER_INCLUDED && rhsType == TRI_AQL_RANGE_UPPER_INCLUDED)) {
      // < && <
      // <= && <=
      if (compareResult > 0) {
        // lhs > rhs
        TRI_FreeAccessAql(rhs);

        return lhs;
      }
      else {
        TRI_FreeAccessAql(lhs);

        return rhs;
      }
    }
    else if (lhsType == TRI_AQL_RANGE_LOWER_EXCLUDED && rhsType == TRI_AQL_RANGE_LOWER_INCLUDED) {
      // > && >=
      if (compareResult >= 0) {
        // lhs > rhs
        TRI_FreeAccessAql(lhs);

        return rhs;
      } 
      else {
        TRI_FreeAccessAql(rhs);

        return lhs;
      }
    }
    else if (lhsType == TRI_AQL_RANGE_UPPER_EXCLUDED && rhsType == TRI_AQL_RANGE_UPPER_INCLUDED) {
      // < && <=
      if (compareResult <= 0) {
        // lhs < rhs
        TRI_FreeAccessAql(lhs);

        return rhs;
      }
      else {
        TRI_FreeAccessAql(rhs);

        return lhs;
      }
    }
  }
  
  // for all other combinations, we give up
  TRI_FreeAccessAql(rhs);
  FreeAccessMembers(lhs);

  lhs->_type = TRI_AQL_ACCESS_ALL;

  return lhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two access structures using a logical OR
///
/// left hand operand is a double range, the result type depends on the right
/// hand operand type
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeOrRangeDouble (TRI_aql_context_t* const context,
                                                   TRI_aql_field_access_t* lhs,
                                                   TRI_aql_field_access_t* rhs) {
  assert(false);
  /* this should never be called. let's see if this assumption is true or not */
  assert(lhs->_type == TRI_AQL_ACCESS_RANGE_DOUBLE);

  return lhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two attribute access structures using logical AND
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeAttributeAccessAnd (TRI_aql_context_t* const context,
                                                        TRI_aql_field_access_t* lhs,
                                                        TRI_aql_field_access_t* rhs) {
  assert(context); 
  assert(lhs); 
  assert(rhs); 
  assert(lhs->_fullName != NULL);
  assert(rhs->_fullName != NULL);

  if (lhs->_type > rhs->_type) {
    // swap operands so they are always sorted
    TRI_aql_field_access_t* tmp = lhs;
    lhs = rhs;
    rhs = tmp;
  }

  assert(lhs->_type <= rhs->_type);

  switch (lhs->_type) {
    case TRI_AQL_ACCESS_IMPOSSIBLE:
      return MergeAndImpossible(context, lhs, rhs);
    case TRI_AQL_ACCESS_ALL:
      return MergeAndAll(context, lhs, rhs);
    case TRI_AQL_ACCESS_REFERENCE:
      return MergeAndReference(context, lhs, rhs);
    case TRI_AQL_ACCESS_EXACT:
      return MergeAndExact(context, lhs, rhs);
    case TRI_AQL_ACCESS_LIST:
      return MergeAndList(context, lhs, rhs);
    case TRI_AQL_ACCESS_RANGE_SINGLE:
      return MergeAndRangeSingle(context, lhs, rhs);
    case TRI_AQL_ACCESS_RANGE_DOUBLE:
      return MergeAndRangeDouble(context, lhs, rhs);
  }

  assert(false);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two attribute access structures using logical OR
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeAttributeAccessOr (TRI_aql_context_t* const context,
                                                       TRI_aql_field_access_t* lhs,
                                                       TRI_aql_field_access_t* rhs) {
  assert(context); 
  assert(lhs); 
  assert(rhs); 
  assert(lhs->_fullName != NULL);
  assert(rhs->_fullName != NULL);

  if (lhs->_type > rhs->_type) {
    // swap operands so they are always sorted
    TRI_aql_field_access_t* tmp = lhs;
    lhs = rhs;
    rhs = tmp;
  }

  assert(lhs->_type <= rhs->_type);
    
  switch (lhs->_type) {
    case TRI_AQL_ACCESS_IMPOSSIBLE:
      return MergeOrImpossible(context, lhs, rhs);
    case TRI_AQL_ACCESS_ALL:
      return MergeOrAll(context, lhs, rhs);
    case TRI_AQL_ACCESS_REFERENCE:
      return MergeOrReference(context, lhs, rhs);
    case TRI_AQL_ACCESS_EXACT:
      return MergeOrExact(context, lhs, rhs);
    case TRI_AQL_ACCESS_LIST:
      return MergeOrList(context, lhs, rhs);
    case TRI_AQL_ACCESS_RANGE_SINGLE:
      return MergeOrRangeSingle(context, lhs, rhs);
    case TRI_AQL_ACCESS_RANGE_DOUBLE:
      return MergeOrRangeDouble(context, lhs, rhs);
  }

  assert(false);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                 attribute access vector functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////
  
static TRI_vector_pointer_t* CreateEmptyVector (TRI_aql_context_t* const context) {
  TRI_vector_pointer_t* result;

  assert(context);

  result = (TRI_vector_pointer_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vector_pointer_t), false);
  if (!result) {
    // OOM
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }
  
  TRI_InitVectorPointer(result, TRI_UNKNOWN_MEM_ZONE);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a vector with an attribute access struct in it
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_pointer_t* Vectorize (TRI_aql_context_t* const context,
                                        TRI_aql_field_access_t* fieldAccess) {
  TRI_vector_pointer_t* vector;

  assert(context);
  if (!fieldAccess) {
    return NULL;
  }

  vector = CreateEmptyVector(context);
  if (!vector) {
    return NULL;
  }

  TRI_PushBackVectorPointer(vector, fieldAccess);

  return vector;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert all attribute access structures in a vector to all items
/// accesses. this is done when a logical NOT is found and we do not now how to
/// handle the negated conditions
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_pointer_t* MakeAllVector (TRI_aql_context_t* const context,
                                            TRI_vector_pointer_t* const fieldAccesses) {
  size_t i, n;

  if (!fieldAccesses) {
    return NULL;
  }

  n = fieldAccesses->_length;
  for (i = 0; i < n; ++i) {
    // turn all field access values into an all items access
    TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(fieldAccesses, i);

    assert(fieldAccess);
    assert(fieldAccess->_fullName);

    // modify the element in place
    FreeAccessMembers(fieldAccess);
    fieldAccess->_type = TRI_AQL_ACCESS_ALL;
  }

  return fieldAccesses;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert the elements of the source vector into the results vector
/// 
/// if an element is already present in the result vector, merge it
////////////////////////////////////////////////////////////////////////////////

static void MergeVector (TRI_aql_context_t* const context,
                         const TRI_aql_node_type_e mergeType,
                         TRI_vector_pointer_t* const result,
                         const TRI_vector_pointer_t* const source) {
  size_t i, n;

  assert(result);
  assert(source);

  n = source->_length;
  for (i = 0; i < n; ++i) {
    TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(source, i);
    size_t j, len;
    bool found = false;

    assert(fieldAccess);
    assert(fieldAccess->_fullName);

    // check if element is in result vector already
    len = result->_length;
    for (j = 0; j < len; ++j) {
      TRI_aql_field_access_t* compareAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(result, j);
    
      assert(compareAccess);
      assert(compareAccess->_fullName);

      if (TRI_EqualString(fieldAccess->_fullName, compareAccess->_fullName)) {
        // found the element
        if (mergeType == TRI_AQL_NODE_OPERATOR_BINARY_AND) {
          result->_buffer[j] = MergeAttributeAccessAnd(context, fieldAccess, compareAccess);
        }
        else {
          result->_buffer[j] = MergeAttributeAccessOr(context, fieldAccess, compareAccess);
        }
      
        found = true;
        break;
      } 
    }

    if (!found) {
      TRI_PushBackVectorPointer(result, fieldAccess);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a copy of all elements from source vector into result vector
////////////////////////////////////////////////////////////////////////////////

static void InsertVector (TRI_aql_context_t* const context,
                          TRI_vector_pointer_t* const result,
                          const TRI_vector_pointer_t* const source) {
  size_t i, n;

  assert(result);
  assert(source);

  n = source->_length;
  for (i = 0; i < n; ++i) {
    TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(source, i);
    TRI_aql_field_access_t* copy;

    assert(fieldAccess);
    assert(fieldAccess->_fullName);

    copy = TRI_CloneAccessAql(context, fieldAccess);

    if (!copy) {
      // OOM
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return;
    }

    TRI_PushBackVectorPointer(result, (void*) copy);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two attribute access vectors using logical AND or OR
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_pointer_t* MergeVectors (TRI_aql_context_t* const context,
                                           const TRI_aql_node_type_e mergeType,
                                           TRI_vector_pointer_t* const lhs,
                                           TRI_vector_pointer_t* const rhs,
                                           const TRI_vector_pointer_t* const inheritedRestrictions) {
  TRI_vector_pointer_t* result;

  assert(context);
  assert(mergeType == TRI_AQL_NODE_OPERATOR_BINARY_AND || mergeType == TRI_AQL_NODE_OPERATOR_BINARY_OR);

  // first check if we can get away without copying/merging the vectors
  if (inheritedRestrictions == NULL) {
    // 3rd vector is empty
    if (lhs != NULL && rhs == NULL) {
      // 2nd vector is also empty, we simply return the 1st one
      // no copying/merging required
      return lhs;
    }
    if (lhs == NULL && rhs != NULL) {
      // 1st vector is also empty, we simply return the 2nd one
      // no copying/merging required
      return rhs;
    }
  }

  result = CreateEmptyVector(context);
  if (!result) {
    // free memory
    if (lhs) {
      TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, lhs);
    }
    if (rhs) {
      TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, rhs);
    }

    // OOM
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return NULL;
  }
  
  if (inheritedRestrictions) {
    // insert a copy of all restrictions first
    InsertVector(context, result, inheritedRestrictions);
  }

  if (lhs) {
    // copy elements from lhs into result vector
    MergeVector(context, mergeType, result, lhs);
    TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, lhs);
  }

  if (rhs) {
    // copy elements from rhs into result vector
    MergeVector(context, mergeType, result, rhs);
    TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, rhs);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                         node processing functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create an access structure for the given node and operator
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* CreateAccessForNode (TRI_aql_context_t* const context,
                                                    const TRI_aql_attribute_name_t* const field, 
                                                    const TRI_aql_node_type_e operator,
                                                    const TRI_aql_node_t* const node) {
  TRI_aql_field_access_t* fieldAccess;
  TRI_json_t* value;

  assert(context);
  assert(field);
  assert(field->_name._buffer);
  assert(node);

  fieldAccess = (TRI_aql_field_access_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_field_access_t), false);
  if (fieldAccess == NULL) {
    // OOM
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return NULL;
  }

  fieldAccess->_fullName = TRI_DuplicateString(field->_name._buffer);
  if (fieldAccess->_fullName == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldAccess);

    return NULL;
  }
  SetNameLength(fieldAccess);
  
  if (operator == TRI_AQL_NODE_OPERATOR_BINARY_NE) {
    // create an all items access, and we're done
    fieldAccess->_type = TRI_AQL_ACCESS_ALL;

    return fieldAccess;
  } 

  // all other operation types require a value...
  value = TRI_NodeJsonAql(context, node);
  if (value == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return NULL;
  }  

  if (operator == TRI_AQL_NODE_OPERATOR_BINARY_EQ) {
    // create an exact value access
    fieldAccess->_type = TRI_AQL_ACCESS_EXACT; 
    fieldAccess->_value._value = value;
  } 
  else if (operator == TRI_AQL_NODE_OPERATOR_BINARY_LT) { 
    // create a single range access
    fieldAccess->_type = TRI_AQL_ACCESS_RANGE_SINGLE;
    fieldAccess->_value._singleRange._type = TRI_AQL_RANGE_UPPER_EXCLUDED;
    fieldAccess->_value._singleRange._value = value;
  } 
  else if (operator == TRI_AQL_NODE_OPERATOR_BINARY_LE) { 
    // create a single range access
    fieldAccess->_type = TRI_AQL_ACCESS_RANGE_SINGLE;
    fieldAccess->_value._singleRange._type = TRI_AQL_RANGE_UPPER_INCLUDED;
    fieldAccess->_value._singleRange._value = value;
  } 
  else if (operator == TRI_AQL_NODE_OPERATOR_BINARY_GT) { 
    // create a single range access
    fieldAccess->_type = TRI_AQL_ACCESS_RANGE_SINGLE;
    fieldAccess->_value._singleRange._type = TRI_AQL_RANGE_LOWER_EXCLUDED;
    fieldAccess->_value._singleRange._value = value;
  } 
  else if (operator == TRI_AQL_NODE_OPERATOR_BINARY_GE) { 
    // create a single range access
    fieldAccess->_type = TRI_AQL_ACCESS_RANGE_SINGLE;
    fieldAccess->_value._singleRange._type = TRI_AQL_RANGE_LOWER_INCLUDED;
    fieldAccess->_value._singleRange._value = value;
  }
  else if (operator == TRI_AQL_NODE_OPERATOR_BINARY_IN) {
    TRI_json_t* list;

    assert(value->_type == TRI_JSON_LIST);

    // create a list access 
    fieldAccess->_type = TRI_AQL_ACCESS_LIST;
    fieldAccess->_value._value = value;

    // check if list contains more than 1 value
    if (value->_value._objects._length > 1) {
      // list contains more than one value, we need to make it unique

      // sort values in list
      TRI_SortListJson(fieldAccess->_value._value);
      // make list values unique, this will create a new list
      list = TRI_UniquifyListJson(fieldAccess->_value._value);
      // free old list
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_value._value);

      // now use the sorted && unique list
      fieldAccess->_value._value = list;
    }
  }
  else {
    assert(false);
  }
  
  return fieldAccess;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an attribute access structure for the given node and 
/// relational operator
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* GetAttributeAccess (TRI_aql_context_t* const context,
                                                   const TRI_aql_attribute_name_t* const field,
                                                   const TRI_aql_node_type_e operator,
                                                   const TRI_aql_node_t* const node) {
  TRI_aql_field_access_t* fieldAccess;
  
  assert(context);
  assert(node);

  if (!field || !field->_name._buffer) {
    // this is ok if the node type is not supported
    return NULL;
  }
    
  fieldAccess = CreateAccessForNode(context, field, operator, node);
  if (!fieldAccess) { 
    // OOM
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }

  return fieldAccess;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set up the name of an attribute, including the variable name and
/// '.'s (e.g. u.birthday.day) for a given node
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_attribute_name_t* GetAttributeName (TRI_aql_context_t* const context,
                                                   const TRI_aql_node_t* const node) {
  assert(context);
  assert(node);

  if (node->_type == TRI_AQL_NODE_ATTRIBUTE_ACCESS) {
    TRI_aql_attribute_name_t* field = GetAttributeName(context, TRI_AQL_NODE_MEMBER(node, 0));

    if (!field) {
      return NULL;
    }

    TRI_AppendCharStringBuffer(&field->_name, '.');
    TRI_AppendStringStringBuffer(&field->_name, TRI_AQL_NODE_STRING(node));
    return field;
  }
  else if (node->_type == TRI_AQL_NODE_REFERENCE) {
    TRI_aql_attribute_name_t* field;
    
    field = (TRI_aql_attribute_name_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_attribute_name_t), false);

    if (!field) {
      // OOM
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
/// @brief process a condition node and recurse into its subnodes
///
/// if an impossible range is found underneath and AND or OR operator, the
/// subnodes will also be modified. if a modification is done, the changed flag
/// is set
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_pointer_t* ProcessNode (TRI_aql_context_t* const context,
                                          TRI_aql_node_t* node,
                                          bool* changed,
                                          const TRI_vector_pointer_t* const inheritedRestrictions) {
  assert(context);
  assert(node);

  if (node->_type == TRI_AQL_NODE_OPERATOR_UNARY_NOT) {
    TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
    
    assert(lhs);
    
    // can ignore inherited restrictions here
    return MakeAllVector(context, ProcessNode(context, lhs, changed, NULL));
  }

  if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_OR ||
      node->_type == TRI_AQL_NODE_OPERATOR_BINARY_AND) {
    TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
    TRI_aql_node_t* rhs = TRI_AQL_NODE_MEMBER(node, 1);
    TRI_vector_pointer_t* result;

    assert(lhs);
    assert(rhs);

    // recurse into next level
    result = MergeVectors(context, 
                          node->_type,
                          ProcessNode(context, lhs, changed, inheritedRestrictions), 
                          ProcessNode(context, rhs, changed, inheritedRestrictions),
                          inheritedRestrictions);

    if (result == NULL) {
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    }
    else {
      if (TRI_ContainsImpossibleAql(result)) {
        // inject a bool(false) node into the true if the condition is always false
        node->_members._buffer[0] = TRI_CreateNodeValueBoolAql(context, false);
        node->_members._buffer[1] = TRI_CreateNodeValueBoolAql(context, false);

        // set changed marker
        *changed = true;
      }
    }

    return result;
  }

  if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_EQ ||
      node->_type == TRI_AQL_NODE_OPERATOR_BINARY_NE ||
      node->_type == TRI_AQL_NODE_OPERATOR_BINARY_LT ||
      node->_type == TRI_AQL_NODE_OPERATOR_BINARY_LE ||
      node->_type == TRI_AQL_NODE_OPERATOR_BINARY_GT ||
      node->_type == TRI_AQL_NODE_OPERATOR_BINARY_GE ||
      node->_type == TRI_AQL_NODE_OPERATOR_BINARY_IN) {
    TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
    TRI_aql_node_t* rhs = TRI_AQL_NODE_MEMBER(node, 1);
    TRI_aql_attribute_name_t* field;
    TRI_aql_node_t* node1;
    TRI_aql_node_t* node2; 
    TRI_aql_node_type_e operator;

    if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_IN && rhs->_type != TRI_AQL_NODE_LIST) {
      // in operator is special. if right operand is not a list, we must abort here
      return NULL;
    } 

    if (lhs->_type == TRI_AQL_NODE_ATTRIBUTE_ACCESS && TRI_IsConstantValueNodeAql(rhs)) {
      // collection.attribute operator value
      node1 = lhs;
      node2 = rhs;
      operator = node->_type;
    }
    else if (rhs->_type == TRI_AQL_NODE_ATTRIBUTE_ACCESS && TRI_IsConstantValueNodeAql(lhs)) {
      // value operator collection.attribute
      node1 = rhs;
      node2 = lhs;
      operator = TRI_ReverseOperatorRelationalAql(node->_type);
      assert(operator != TRI_AQL_NODE_NOP);
    }
    else if (lhs->_type == TRI_AQL_NODE_REFERENCE && TRI_IsConstantValueNodeAql(rhs)) {
      // variable operator value
      node1 = lhs;
      node2 = rhs;
      operator = node->_type;
    }
    else if (rhs->_type == TRI_AQL_NODE_REFERENCE && TRI_IsConstantValueNodeAql(lhs)) {
      // value operator variable
      node1 = rhs;
      node2 = lhs;
      operator = TRI_ReverseOperatorRelationalAql(node->_type);
      assert(operator != TRI_AQL_NODE_NOP);
    }
    else {
      return NULL;
    } 

    if (node2->_type != TRI_AQL_NODE_VALUE && 
        node2->_type != TRI_AQL_NODE_LIST &&
        node2->_type != TRI_AQL_NODE_ARRAY) {
      // only the above types are supported
      return NULL;
    } 

    field = GetAttributeName(context, node1);

    if (field) {
      TRI_aql_field_access_t* attributeAccess = GetAttributeAccess(context, field, operator, node2);
      TRI_vector_pointer_t* result;

      TRI_DestroyStringBuffer(&field->_name);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, field);
       
      result = MergeVectors(context,
                            TRI_AQL_NODE_OPERATOR_BINARY_AND,
                            Vectorize(context, attributeAccess),
                            NULL,
                            inheritedRestrictions);
    
      if (result == NULL) {
        TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      }
      else {
        if (TRI_ContainsImpossibleAql(result)) {
          // inject a dummy false == true node into the true if the condition is always false
          node->_type = TRI_AQL_NODE_OPERATOR_BINARY_EQ;
          node->_members._buffer[0] = TRI_CreateNodeValueBoolAql(context, false);
          node->_members._buffer[1] = TRI_CreateNodeValueBoolAql(context, true);

          // set changed marker
          *changed = true;
        }
      }

      return result;
    }
  }

  return NULL;
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
/// @brief create an access structure of type impossible
////////////////////////////////////////////////////////////////////////////////

TRI_aql_field_access_t* TRI_CreateImpossibleAccessAql (TRI_aql_context_t* const context) {
  return CreateFieldAccess(context, TRI_AQL_ACCESS_IMPOSSIBLE, "unused");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if an attribute access structure vector contains the
/// impossible range 
////////////////////////////////////////////////////////////////////////////////

bool TRI_ContainsImpossibleAql (const TRI_vector_pointer_t* const fieldAccesses) {
  size_t i, n;

  if (!fieldAccesses) {
    return false;
  }

  n = fieldAccesses->_length;
  for (i = 0; i < n; ++i) {
    TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(fieldAccesses, i);
    
    assert(fieldAccess);

    if (fieldAccess->_type == TRI_AQL_ACCESS_IMPOSSIBLE) {
      // impossible range found
      return true;
    }
  }

  // impossible range not found
  return false;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief clone a vector of accesses
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_CloneAccessesAql (TRI_aql_context_t* const context, 
                                            const TRI_vector_pointer_t* const source) {
  TRI_vector_pointer_t* result;
 
  if (!source) {
   return NULL;
  }
 
  result = CreateEmptyVector(context);
  if (!result) {
    return NULL;
  }

  InsertVector(context, result, source);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free access structure with its members and the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeAccessAql (TRI_aql_field_access_t* const fieldAccess) {
  assert(fieldAccess);
  
  FreeAccessMembers(fieldAccess);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_fullName);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldAccess);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone an attribute access structure by deep-copying it
////////////////////////////////////////////////////////////////////////////////

TRI_aql_field_access_t* TRI_CloneAccessAql (TRI_aql_context_t* const context,
                                            TRI_aql_field_access_t* const source) {
  TRI_aql_field_access_t* fieldAccess;

  assert(source);
  assert(source->_fullName);
 
  fieldAccess = CreateFieldAccess(context, source->_type, source->_fullName); 
  if (fieldAccess == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }

  switch (source->_type) {
    case TRI_AQL_ACCESS_EXACT:
    case TRI_AQL_ACCESS_LIST: {
      fieldAccess->_value._value = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, source->_value._value);
      if (fieldAccess->_value._value == NULL) {
        // OOM
        TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      }
      break;
    }

    case TRI_AQL_ACCESS_RANGE_SINGLE: {
      fieldAccess->_value._singleRange._type = source->_value._singleRange._type;
      fieldAccess->_value._singleRange._value = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, source->_value._singleRange._value);
      if (fieldAccess->_value._singleRange._value == NULL) {
        // OOM
        TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      }
      break;
    }

    case TRI_AQL_ACCESS_RANGE_DOUBLE: {
      fieldAccess->_value._between._lower._type = source->_value._between._lower._type;
      fieldAccess->_value._between._upper._type = source->_value._between._upper._type;
      fieldAccess->_value._between._lower._value = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, source->_value._between._lower._value);
      fieldAccess->_value._between._upper._value = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, source->_value._between._upper._value);
      if (fieldAccess->_value._between._lower._value == NULL ||
          fieldAccess->_value._between._upper._value == NULL) {
        // OOM
        TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      }
      break;
    }
    
    case TRI_AQL_ACCESS_REFERENCE: {
      // TODO
      break;
    }

    case TRI_AQL_ACCESS_ALL:
    case TRI_AQL_ACCESS_IMPOSSIBLE: {
      // nada
      break;
    }
  }

  return fieldAccess;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the preferred (i.e. better) access type for a loop
////////////////////////////////////////////////////////////////////////////////

int TRI_PickAccessAql (const TRI_aql_field_access_t* const lhs,
                       const TRI_aql_field_access_t* const rhs) {
  if (!lhs) {
    // lhs does not exist, simply return other side
    return 1;
  }

  if (!rhs) {
    // rhs does not exist, simply return other side
    return -1;
  }

  if (lhs->_type < rhs->_type) {
    // lhs is more efficient than rhs
    return -1;
  }

  if (lhs->_type > rhs->_type) {
    // rhs is more efficient than lhs
    return 1;
  }

  // efficiency class of lhs and rhs is equal
  if (lhs->_type == TRI_AQL_ACCESS_LIST) {
    size_t l, r;

    l = lhs->_value._value->_value._objects._length;
    r = rhs->_value._value->_value._objects._length;

    // for lists, compare number of elements
    if (l < r) {
      // lhs list has less elements than rhs list
      return -1;
    }
    else if (l > r) {
      // lhs list has more elements than rhs list
      return 1;
    }
  }

  // we cannot determine which side is better. let the client decide
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump a single acces for debugging purposes
////////////////////////////////////////////////////////////////////////////////

#if TRI_DEBUG_AQL
void TRI_DumpAccessAql (const TRI_aql_field_access_t* const fieldAccess) {
  printf("\nFIELD ACCESS\n- FIELD: %s\n",fieldAccess->_fullName);
  printf("- TYPE: %s\n", AccessName(fieldAccess->_type));
  
  if (fieldAccess->_type == TRI_AQL_ACCESS_EXACT || fieldAccess->_type == TRI_AQL_ACCESS_LIST) {
    TRI_string_buffer_t b;
    TRI_InitStringBuffer(&b, TRI_UNKNOWN_MEM_ZONE);
    TRI_StringifyJson(&b, fieldAccess->_value._value);

    printf("- VALUE: %s\n", b._buffer);
    TRI_DestroyStringBuffer(&b);
  }
  else if (fieldAccess->_type == TRI_AQL_ACCESS_RANGE_SINGLE) {
    TRI_string_buffer_t b;
    TRI_InitStringBuffer(&b, TRI_UNKNOWN_MEM_ZONE);
    TRI_StringifyJson(&b, fieldAccess->_value._singleRange._value);

    printf("- VALUE: %s\n", b._buffer);
    TRI_DestroyStringBuffer(&b);
  }
  else if (fieldAccess->_type == TRI_AQL_ACCESS_RANGE_DOUBLE) {
    TRI_string_buffer_t b;
    TRI_InitStringBuffer(&b, TRI_UNKNOWN_MEM_ZONE);
    TRI_StringifyJson(&b, fieldAccess->_value._between._lower._value);
    TRI_AppendStringStringBuffer(&b, ", ");
    TRI_StringifyJson(&b, fieldAccess->_value._between._upper._value);

    printf("- VALUE: %s\n", b._buffer);
    TRI_DestroyStringBuffer(&b);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump ranges found for debugging purposes
////////////////////////////////////////////////////////////////////////////////

void TRI_DumpAccessesAql (const TRI_vector_pointer_t* const ranges) {
  size_t i, n; 
  
  assert(ranges);
   
  n = ranges->_length;
  for (i = 0; i < n; ++i) {
    TRI_aql_field_access_t* fieldAccess = TRI_AtVectorPointer(ranges, i);

    TRI_DumpAccessesAql(fieldAccess);
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief free all field access structs in a vector
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeAccessesAql (TRI_vector_pointer_t* const fieldAccesses) {
  size_t i, n;

  n = fieldAccesses->_length;
  for (i = 0; i < n; ++i) {
    TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(fieldAccesses, i);

    TRI_FreeAccessAql(fieldAccess);
  }

  // free vector
  TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, fieldAccesses);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a field access type to an existing field access vector
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_AddAccessAql (TRI_aql_context_t* const context,
                                        TRI_vector_pointer_t* const previous,
                                        TRI_aql_field_access_t* const candidate) {
  TRI_vector_pointer_t* accesses = NULL;
  size_t i, n;
  bool found;

  assert(context);
  assert(candidate);
  assert(candidate->_fullName);
  
  if (previous != NULL) {
    // use existing vector if already available
    accesses = previous;
  }
  else {
    // create a new vector 
    accesses = CreateEmptyVector(context);
    if (accesses == NULL) {
      // OOM
      return NULL;
    }
  }

  assert(accesses);

  found = false;
  n = accesses->_length;
  for (i = 0; i < n; ++i) {
    TRI_aql_field_access_t* existing = (TRI_aql_field_access_t*) TRI_AtVectorPointer(accesses, i);
    TRI_aql_field_access_t* copy;
    int result;

    if (!TRI_EqualString(candidate->_fullName, existing->_fullName)) {
      continue;
    }
    
    // we found a match
    found = true;
    
    result = TRI_PickAccessAql(candidate, existing);
    if (result < 0) {
      // candidate is preferred

      // free existing field access
      assert(existing);
      TRI_FreeAccessAql(existing);
      copy = TRI_CloneAccessAql(context, candidate);
      if (copy == NULL) {
        // OOM
        TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
        return accesses;
      }

      // insert copy of candidate instead
      accesses->_buffer[i] = (void*) copy;
    }
    break;
  }

  if (!found) {
    // not found, now add this candidate
    TRI_PushBackVectorPointer(accesses, TRI_CloneAccessAql(context, candidate)); 
  }

  return accesses;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the range operator string for a range operator
////////////////////////////////////////////////////////////////////////////////

const char* TRI_RangeOperatorAql (const TRI_aql_range_e type) {
  switch (type) {
    case TRI_AQL_RANGE_LOWER_EXCLUDED:
      return ">";
    case TRI_AQL_RANGE_LOWER_INCLUDED:
      return ">=";
    case TRI_AQL_RANGE_UPPER_EXCLUDED:
      return "<";
    case TRI_AQL_RANGE_UPPER_INCLUDED:
      return "<=";
  }
 
  assert(false);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief track and optimise attribute accesses for a given node and subnodes
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_OptimiseRangesAql (TRI_aql_context_t* const context,
                                             TRI_aql_node_t* node,
                                             bool* changed,
                                             const TRI_vector_pointer_t* const inheritedRestrictions) {
  return ProcessNode(context, node, changed, inheritedRestrictions);
}
  
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, access optimizer
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

#include "Ahuacatl/ahuacatl-access-optimizer.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hash a field name
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashFieldAccess (TRI_associative_pointer_t* array, 
                               void const* element) {
  TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) element;

  return TRI_FnvHashString(fieldAccess->_fieldName);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief comparison function used to determine field name equality
////////////////////////////////////////////////////////////////////////////////

static bool EqualFieldAccess (TRI_associative_pointer_t* array, 
                              void const* key, 
                              void const* element) {
  TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) element;

  return TRI_EqualString(key, fieldAccess->_fieldName);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the logical type for a sub operation
////////////////////////////////////////////////////////////////////////////////

static inline TRI_aql_logical_e SubOperator (const TRI_aql_logical_e preferredType,
                                             const TRI_aql_logical_e parentType) {
  if (parentType == TRI_AQL_LOGICAL_NOT) {
    // logical NOT is sticky
    return parentType;
  }

  // all other operators are not
  return preferredType;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the name of an access type
////////////////////////////////////////////////////////////////////////////////

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
    case TRI_AQL_ACCESS_RANGE_SINGLE: 
      return "single range";
    case TRI_AQL_ACCESS_RANGE_DOUBLE: 
      return "double range";
    default:
      return "unknown";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free access member data, but do not free the access struct itself
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
    case TRI_AQL_ACCESS_RANGE_SINGLE:
      if (fieldAccess->_value._singleRange._value) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_value._singleRange._value);
      }
      break;
    case TRI_AQL_ACCESS_RANGE_DOUBLE:
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
/// @brief free access structure with its members and the pointer
////////////////////////////////////////////////////////////////////////////////

static void FreeAccess (TRI_aql_context_t* const context,
                        TRI_aql_field_access_t* const fieldAccess) {
  assert(fieldAccess);

  FreeAccessMembers(fieldAccess);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_fieldName);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldAccess);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two access structures using a logical NOT
///
/// this always returns the all items range because we cannot evaluate the
/// negated condition
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeNot (TRI_aql_context_t* const context,
                                         TRI_aql_field_access_t* lhs,
                                         TRI_aql_field_access_t* rhs) {
  // always returns all
  FreeAccess(context, rhs);
  FreeAccessMembers(lhs);

  lhs->_type = TRI_AQL_ACCESS_ALL;

  return lhs;
}
  
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
  FreeAccess(context, rhs);

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
  FreeAccess(context, lhs);

  return rhs;
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

    FreeAccess(context, rhs);

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

    FreeAccess(context, rhs);

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
      FreeAccess(context, rhs);
      FreeAccessMembers(lhs);
      lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

      return lhs;
    }

    // lhs value is contained in rhs range, simply return rhs
    FreeAccess(context, lhs);

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
      FreeAccess(context, rhs);
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
      FreeAccess(context, rhs);
      FreeAccessMembers(lhs);
      lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

      return lhs;
    }

    // lhs value is contained in rhs range, return rhs
    FreeAccess(context, lhs);

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
    FreeAccess(context, rhs);

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
    FreeAccess(context, rhs);

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
    FreeAccess(context, rhs);

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
        FreeAccess(context, rhs);

        return lhs;
      }
      else {
        FreeAccess(context, lhs);

        return rhs;
      }
    }
    else if ((lhsType == TRI_AQL_RANGE_UPPER_EXCLUDED && rhsType == TRI_AQL_RANGE_UPPER_EXCLUDED) ||
        (lhsType == TRI_AQL_RANGE_UPPER_INCLUDED && rhsType == TRI_AQL_RANGE_UPPER_INCLUDED)) {
      // < && <
      // <= && <=
      if (compareResult > 0) {
        // lhs > rhs
        FreeAccess(context, lhs);

        return rhs;
      }
      else {
        FreeAccess(context, rhs);

        return lhs;
      }
    }
    else if (lhsType == TRI_AQL_RANGE_LOWER_EXCLUDED && rhsType == TRI_AQL_RANGE_LOWER_INCLUDED) {
      // > && >=
      if (compareResult >= 0) {
        // lhs > rhs
        FreeAccess(context, rhs);

        return lhs;
      } 
      else {
        FreeAccess(context, lhs);

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
        FreeAccess(context, rhs); 

        lhs->_type = TRI_AQL_ACCESS_RANGE_DOUBLE;
        lhs->_value._between._lower._type = lhsType;
        lhs->_value._between._lower._value = lhsValue;
        lhs->_value._between._upper._type = rhsType;
        lhs->_value._between._upper._value = rhsValue;

        return lhs;
      }
      else {
        FreeAccess(context, rhs);
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
        FreeAccess(context, rhs); 

        lhs->_type = TRI_AQL_ACCESS_RANGE_DOUBLE;
        lhs->_value._between._lower._type = lhsType;
        lhs->_value._between._lower._value = lhsValue;
        lhs->_value._between._upper._type = rhsType;
        lhs->_value._between._upper._value = rhsValue;

        return lhs;
      }
      else {
        FreeAccess(context, rhs);
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
        FreeAccess(context, rhs); 

        lhs->_type = TRI_AQL_ACCESS_RANGE_DOUBLE;
        lhs->_value._between._lower._type = lhsType;
        lhs->_value._between._lower._value = lhsValue;
        lhs->_value._between._upper._type = rhsType;
        lhs->_value._between._upper._value = rhsValue;

        return lhs;
      }
      else if (compareResult == 0) {
        FreeAccess(context, rhs);

        // save pointer
        lhsValue = lhs->_value._singleRange._value;
        lhs->_type = TRI_AQL_ACCESS_EXACT;
        lhs->_value._value = lhsValue;
        return lhs;
      }
      else {
        FreeAccess(context, rhs);
        FreeAccessMembers(lhs);
        lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

        return lhs;
      }
    }
    else if (lhsType == TRI_AQL_RANGE_UPPER_EXCLUDED && rhsType == TRI_AQL_RANGE_UPPER_INCLUDED) {
      // < && <=
      if (compareResult <= 0) {
        FreeAccess(context, rhs);

        return lhs;
      }
      else {
        FreeAccess(context, lhs);

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
        FreeAccess(context, rhs);
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

      FreeAccess(context, lhs);

      return rhs;
    }
    
    if (lhs->_value._singleRange._type == TRI_AQL_RANGE_LOWER_INCLUDED) {
      compareResult = TRI_CompareValuesJson(lhs->_value._singleRange._value, rhs->_value._between._upper._value);
      if (compareResult > 0 || (compareResult == 0 && rhs->_value._between._upper._type == TRI_AQL_RANGE_UPPER_EXCLUDED)) {
        // lhs value is bigger than rhs upper bound
        FreeAccess(context, rhs);
        FreeAccessMembers(lhs);

        lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

        return lhs;
      }
      else if (compareResult == 0) {
        // save pointer
        TRI_json_t* value = lhs->_value._singleRange._value;
        
        FreeAccess(context, rhs);
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

      FreeAccess(context, lhs);

      return rhs;
    }
    
    if (lhs->_value._singleRange._type == TRI_AQL_RANGE_UPPER_EXCLUDED) {
      compareResult = TRI_CompareValuesJson(lhs->_value._singleRange._value, rhs->_value._between._lower._value);
      if (compareResult <= 0) {
        // lhs value is smaller than rhs lower bound
        FreeAccess(context, rhs);
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

      FreeAccess(context, lhs);

      return rhs;
    }
    
    if (lhs->_value._singleRange._type == TRI_AQL_RANGE_UPPER_INCLUDED) {
      compareResult = TRI_CompareValuesJson(lhs->_value._singleRange._value, rhs->_value._between._lower._value);
      if (compareResult < 0 || (compareResult == 0 && rhs->_value._between._lower._type == TRI_AQL_RANGE_LOWER_EXCLUDED)) {
        // lhs value is smaller than rhs lower bound
        FreeAccess(context, rhs);
        FreeAccessMembers(lhs);
        lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

        return lhs;
      }
      else if (compareResult == 0) {
        // save pointer
        TRI_json_t* value = lhs->_value._singleRange._value;
        
        FreeAccess(context, rhs);
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

      FreeAccess(context, lhs);

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
  FreeAccess(context, lhs);

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
  FreeAccess(context, rhs);

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
      FreeAccess(context, rhs);
    
      return lhs;
    }

    // make a list with both values
    result = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
    if (!result) {
      // OOM
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      FreeAccess(context, rhs);

      return lhs;
    }

    TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, lhs->_value._value);
    TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, rhs->_value._value);
    TRI_SortListJson(result);

    FreeAccess(context, lhs);
    FreeAccessMembers(rhs);

    rhs->_type = TRI_AQL_ACCESS_LIST;
    rhs->_value._value = result;

    return rhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_LIST) {
    // check if lhs is contained in rhs list
    if (TRI_CheckInListJson(lhs->_value._value, rhs->_value._value)) {
      // lhs is contained in rhs, we can return rhs
      FreeAccess(context, lhs);

      return rhs;
    }

    // lhs is not contained, we need to add it to the list
    TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, rhs->_value._value, lhs->_value._value);
    TRI_SortListJson(rhs->_value._value);

    FreeAccess(context, lhs);

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
      FreeAccess(context, rhs);
      FreeAccessMembers(lhs);
      lhs->_type = TRI_AQL_ACCESS_ALL;

      return lhs;
    }

    // lhs value is contained in rhs range, simply return rhs
    FreeAccess(context, lhs);

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
      FreeAccess(context, rhs);
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
      FreeAccess(context, rhs);
      FreeAccessMembers(lhs);
      lhs->_type = TRI_AQL_ACCESS_ALL;

      return lhs;
    }

    // lhs value is contained in rhs range, return rhs
    FreeAccess(context, lhs);

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
    FreeAccess(context, rhs);

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
  FreeAccess(context, rhs);
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
        FreeAccess(context, lhs);

        return rhs;
      }
      else {
        FreeAccess(context, rhs);

        return lhs;
      }
    }
    else if ((lhsType == TRI_AQL_RANGE_UPPER_EXCLUDED && rhsType == TRI_AQL_RANGE_UPPER_EXCLUDED) ||
        (lhsType == TRI_AQL_RANGE_UPPER_INCLUDED && rhsType == TRI_AQL_RANGE_UPPER_INCLUDED)) {
      // < && <
      // <= && <=
      if (compareResult > 0) {
        // lhs > rhs
        FreeAccess(context, rhs);

        return lhs;
      }
      else {
        FreeAccess(context, lhs);

        return rhs;
      }
    }
    else if (lhsType == TRI_AQL_RANGE_LOWER_EXCLUDED && rhsType == TRI_AQL_RANGE_LOWER_INCLUDED) {
      // > && >=
      if (compareResult >= 0) {
        // lhs > rhs
        FreeAccess(context, lhs);

        return rhs;
      } 
      else {
        FreeAccess(context, rhs);

        return lhs;
      }
    }
    else if (lhsType == TRI_AQL_RANGE_UPPER_EXCLUDED && rhsType == TRI_AQL_RANGE_UPPER_INCLUDED) {
      // < && <=
      if (compareResult <= 0) {
        // lhs < rhs
        FreeAccess(context, lhs);

        return rhs;
      }
      else {
        FreeAccess(context, rhs);

        return lhs;
      }
    }
  }
  
  // for all other combinations, we give up
  FreeAccess(context, rhs);
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
/// @brief merge two access structures using either logical AND or OR
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeAccess (TRI_aql_context_t* const context,
                                            const TRI_aql_logical_e logicalType, 
                                            TRI_aql_field_access_t* lhs,
                                            TRI_aql_field_access_t* rhs) {
  assert(context);
  assert(lhs);
  assert(rhs);
  assert(logicalType == TRI_AQL_LOGICAL_AND || 
         logicalType == TRI_AQL_LOGICAL_OR ||
         logicalType == TRI_AQL_LOGICAL_NOT);

  assert(lhs->_fieldName != NULL);
  assert(rhs->_fieldName != NULL);

  if (logicalType == TRI_AQL_LOGICAL_NOT) {
    // logical NOT is simple. we simply turn everything into an all range
    return MergeNot(context, lhs, rhs);
  }

  if (lhs->_type > rhs->_type) {
    // swap operands so they are always sorted
    TRI_aql_field_access_t* tmp = lhs;
    lhs = rhs;
    rhs = tmp;
  }

  assert(lhs->_type <= rhs->_type);

  if (logicalType == TRI_AQL_LOGICAL_AND) {
    // logical AND
    switch (lhs->_type) {
      case TRI_AQL_ACCESS_IMPOSSIBLE:
        return MergeAndImpossible(context, lhs, rhs);
      case TRI_AQL_ACCESS_ALL:
        return MergeAndAll(context, lhs, rhs);
      case TRI_AQL_ACCESS_EXACT:
        return MergeAndExact(context, lhs, rhs);
      case TRI_AQL_ACCESS_LIST:
        return MergeAndList(context, lhs, rhs);
      case TRI_AQL_ACCESS_RANGE_SINGLE:
        return MergeAndRangeSingle(context, lhs, rhs);
      case TRI_AQL_ACCESS_RANGE_DOUBLE:
        return MergeAndRangeDouble(context, lhs, rhs);
    }
  }
  else {
    // logical OR
    switch (lhs->_type) {
      case TRI_AQL_ACCESS_IMPOSSIBLE:
        return MergeOrImpossible(context, lhs, rhs);
      case TRI_AQL_ACCESS_ALL:
        return MergeOrAll(context, lhs, rhs);
      case TRI_AQL_ACCESS_EXACT:
        return MergeOrExact(context, lhs, rhs);
      case TRI_AQL_ACCESS_LIST:
        return MergeOrList(context, lhs, rhs);
      case TRI_AQL_ACCESS_RANGE_SINGLE:
        return MergeOrRangeSingle(context, lhs, rhs);
      case TRI_AQL_ACCESS_RANGE_DOUBLE:
        return MergeOrRangeDouble(context, lhs, rhs);
    }
  }

  assert(false);
  return NULL;
}

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

  value = TRI_NodeJsonAql(context, node);
  if (!value) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }  

  fieldAccess = (TRI_aql_field_access_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_field_access_t), false);
  if (fieldAccess == NULL) {
    // OOM
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return NULL;
  }

  fieldAccess->_fieldName = TRI_DuplicateString(field->_name._buffer);
  if (fieldAccess->_fieldName == NULL) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldAccess);
    return NULL;
  }

  if (operator == AQL_NODE_OPERATOR_BINARY_EQ) {
    // create an exact value access
    fieldAccess->_type = TRI_AQL_ACCESS_EXACT; 
    fieldAccess->_value._value = value;
  } 
  else if (operator == AQL_NODE_OPERATOR_BINARY_LT) { 
    // create a single range access
    fieldAccess->_type = TRI_AQL_ACCESS_RANGE_SINGLE;
    fieldAccess->_value._singleRange._type = TRI_AQL_RANGE_UPPER_EXCLUDED;
    fieldAccess->_value._singleRange._value = value;
  } 
  else if (operator == AQL_NODE_OPERATOR_BINARY_LE) { 
    // create a single range access
    fieldAccess->_type = TRI_AQL_ACCESS_RANGE_SINGLE;
    fieldAccess->_value._singleRange._type = TRI_AQL_RANGE_UPPER_INCLUDED;
    fieldAccess->_value._singleRange._value = value;
  } 
  else if (operator == AQL_NODE_OPERATOR_BINARY_GT) { 
    // create a single range access
    fieldAccess->_type = TRI_AQL_ACCESS_RANGE_SINGLE;
    fieldAccess->_value._singleRange._type = TRI_AQL_RANGE_LOWER_EXCLUDED;
    fieldAccess->_value._singleRange._value = value;
  } 
  else if (operator == AQL_NODE_OPERATOR_BINARY_GE) { 
    // create a single range access
    fieldAccess->_type = TRI_AQL_ACCESS_RANGE_SINGLE;
    fieldAccess->_value._singleRange._type = TRI_AQL_RANGE_LOWER_INCLUDED;
    fieldAccess->_value._singleRange._value = value;
  }
  else if (operator == AQL_NODE_OPERATOR_BINARY_IN) {
    TRI_json_t* list;

    // create a list access 
    fieldAccess->_type = TRI_AQL_ACCESS_LIST;
    fieldAccess->_value._value = value;

    // sort values in list
    TRI_SortListJson(fieldAccess->_value._value);
    // make list values unique
    list = TRI_UniquifyListJson(fieldAccess->_value._value);
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_value._value);
    fieldAccess->_value._value = list;
  }
  else {
    assert(false);
  }
  
  return fieldAccess;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an access structure for the given node and operator,
/// merge it with potential others already found for the same variable
////////////////////////////////////////////////////////////////////////////////

static void NoteAttributeAccess (TRI_aql_context_t* const context,
                                 const TRI_aql_logical_e logicalType,
                                 const TRI_aql_attribute_name_t* const field,
                                 const TRI_aql_node_type_e operator,
                                 const TRI_aql_node_t* const node) {
  TRI_aql_field_access_t* previous;
  TRI_aql_field_access_t* fieldAccess;
  
  assert(context);
  assert(logicalType == TRI_AQL_LOGICAL_AND || 
         logicalType == TRI_AQL_LOGICAL_OR ||
         logicalType == TRI_AQL_LOGICAL_NOT);
  assert(node);

  if (!field || !field->_name._buffer) {
    return;
  }
    
  fieldAccess = CreateAccessForNode(context, field, operator, node);
  if (!fieldAccess) { 
    // OOM
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return;
  }

  // look up previous range first
  previous = (TRI_aql_field_access_t*) TRI_LookupByKeyAssociativePointer(context->_ranges, fieldAccess->_fieldName);
  if (previous) {
    TRI_aql_field_access_t* merged;
    // previous range exists, now merge new access type with previous one
    
    // remove from hash first
    TRI_RemoveKeyAssociativePointer(context->_ranges, fieldAccess->_fieldName);

    // MergeAccess() will free previous and/or fieldAccess
    merged = MergeAccess(context, logicalType, fieldAccess, previous);
    
    if (!merged) {
      // OOM
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return;
    }

    TRI_InsertKeyAssociativePointer(context->_ranges, merged->_fieldName, merged, true);
  }
  else {
    // no previous access exists, no need to merge
    TRI_InsertKeyAssociativePointer(context->_ranges, fieldAccess->_fieldName, fieldAccess, false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set up the name of an attribute, including the variable name and
/// '.'s (e.g. u.birthday.day) for a given node
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_attribute_name_t* GetAttributeName (TRI_aql_context_t* const context,
                                                   const TRI_aql_node_t* const node) {
  assert(context);
  assert(node);

  if (node->_type == AQL_NODE_ATTRIBUTE_ACCESS) {
    TRI_aql_attribute_name_t* field = GetAttributeName(context, TRI_AQL_NODE_MEMBER(node, 0));

    if (!field) {
      return NULL;
    }

    TRI_AppendCharStringBuffer(&field->_name, '.');
    TRI_AppendStringStringBuffer(&field->_name, TRI_AQL_NODE_STRING(node));
    return field;
  }
  else if (node->_type == AQL_NODE_REFERENCE) {
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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief init the optimizer
////////////////////////////////////////////////////////////////////////////////

bool TRI_InitOptimizerAql (TRI_aql_context_t* const context) {
  assert(context);
  assert(context->_ranges == NULL);

  context->_ranges = (TRI_associative_pointer_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_associative_pointer_t), false);
  if (!context->_ranges) {
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
    return false;
  }

  TRI_InitAssociativePointer(context->_ranges,
                             TRI_UNKNOWN_MEM_ZONE, 
                             &TRI_HashStringKeyAssociativePointer, 
                             &HashFieldAccess,
                             &EqualFieldAccess,
                             NULL);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown the optimizer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeOptimizerAql (TRI_aql_context_t* const context) {
  assert(context);

  if (context->_ranges) {
    size_t i, n;

    // free all remaining access elements
    n = context->_ranges->_nrAlloc;
    for (i = 0; i < n; ++i) {
      TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) context->_ranges->_table[i];
      if (!fieldAccess) {
        continue;
      }

      FreeAccess(context, fieldAccess);
    }

    // free hash array
    TRI_FreeAssociativePointer(TRI_UNKNOWN_MEM_ZONE, context->_ranges);
    context->_ranges = NULL;
  }
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
/// @brief dump ranges found for debugging purposes
////////////////////////////////////////////////////////////////////////////////

void TRI_DumpRangesAql (TRI_aql_context_t* const context) {
  size_t i; 
  
  assert(context);
   
  for (i = 0; i < context->_ranges->_nrAlloc; ++i) {
    TRI_aql_field_access_t* fieldAccess = context->_ranges->_table[i]; 

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
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief inspect a condition and note all accesses found for it
////////////////////////////////////////////////////////////////////////////////

void TRI_InspectConditionAql (TRI_aql_context_t* const context,
                              const TRI_aql_logical_e logicalType,
                              TRI_aql_node_t* node) {
  assert(context);
  assert(logicalType == TRI_AQL_LOGICAL_AND ||
         logicalType == TRI_AQL_LOGICAL_OR ||
         logicalType == TRI_AQL_LOGICAL_NOT);

  if (node->_type == AQL_NODE_OPERATOR_UNARY_NOT) {
    TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
    
    assert(lhs);
    
    TRI_InspectConditionAql(context, TRI_AQL_LOGICAL_NOT, lhs);
    return;
  }

  if (node->_type == AQL_NODE_OPERATOR_BINARY_OR) {
    TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
    TRI_aql_node_t* rhs = TRI_AQL_NODE_MEMBER(node, 1);
    TRI_aql_logical_e nextOperator;

    assert(lhs);
    assert(rhs);

    // recurse into next level
    nextOperator = SubOperator(TRI_AQL_LOGICAL_OR, logicalType);
    TRI_InspectConditionAql(context, nextOperator, lhs);
    TRI_InspectConditionAql(context, nextOperator, rhs);
    return;
  }

  if (node->_type == AQL_NODE_OPERATOR_BINARY_AND) {
    TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
    TRI_aql_node_t* rhs = TRI_AQL_NODE_MEMBER(node, 1);
    TRI_aql_logical_e nextOperator;
    
    assert(lhs);
    assert(rhs);

    // recurse into next level
    nextOperator = SubOperator(TRI_AQL_LOGICAL_AND, logicalType);
    TRI_InspectConditionAql(context, nextOperator, lhs);
    TRI_InspectConditionAql(context, nextOperator, rhs);
    return;
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
      TRI_aql_attribute_name_t* field = GetAttributeName(context, lhs);

      if (field) {
        NoteAttributeAccess(context, logicalType, field, node->_type, rhs);
        TRI_DestroyStringBuffer(&field->_name);
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, field);
      }
    }
    else if (rhs->_type == AQL_NODE_ATTRIBUTE_ACCESS) {
      TRI_aql_attribute_name_t* field = GetAttributeName(context, rhs);

      if (field) {
        NoteAttributeAccess(context, logicalType, field, node->_type, lhs);
        TRI_DestroyStringBuffer(&field->_name);
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, field);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

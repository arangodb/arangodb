////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, access optimiser
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ahuacatl-access-optimiser.h"

#include "BasicsC/json.h"
#include "BasicsC/json-utilities.h"
#include "BasicsC/logging.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/tri-strings.h"

#include "Ahuacatl/ahuacatl-context.h"
#include "Ahuacatl/ahuacatl-conversions.h"
#include "Ahuacatl/ahuacatl-functions.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static TRI_aql_attribute_name_t* GetAttributeName (TRI_aql_context_t* const,
                                                   const TRI_aql_node_t* const);

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief structure to contain stringified conditions, used for OR merging
////////////////////////////////////////////////////////////////////////////////

typedef struct or_element_s {
  char*  _name;      // field name, e.g. "u.name"
  char*  _condition; // stringified condition
  size_t _count;     // number of times the same condition occurred
}
or_element_t;

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

////////////////////////////////////////////////////////////////////////////////
/// @brief get the type name for a field access type
////////////////////////////////////////////////////////////////////////////////

static const char* TypeName (const TRI_aql_access_e type) {
  switch (type) {
    case TRI_AQL_ACCESS_IMPOSSIBLE:
      return "impossible";
    case TRI_AQL_ACCESS_EXACT:
      return "exact match";
    case TRI_AQL_ACCESS_LIST:
      return "list match";
    case TRI_AQL_ACCESS_RANGE_SINGLE:
      return "single range";
    case TRI_AQL_ACCESS_RANGE_DOUBLE:
      return "double range";
    case TRI_AQL_ACCESS_REFERENCE:
      return "reference";
    case TRI_AQL_ACCESS_ALL:
      return "all";
  }

  return "unknown";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the type name for a range access
////////////////////////////////////////////////////////////////////////////////

static const char* RangeName (const TRI_aql_range_e type) {
  switch (type) {
    case TRI_AQL_RANGE_LOWER_EXCLUDED:
      return "lower (exc)";
    case TRI_AQL_RANGE_LOWER_INCLUDED:
      return "lower (inc)";
    case TRI_AQL_RANGE_UPPER_EXCLUDED:
      return "upper (exc)";
    case TRI_AQL_RANGE_UPPER_INCLUDED:
      return "upper (inc)";
  }

  return "unknown";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a json value
////////////////////////////////////////////////////////////////////////////////

static void StringifyFieldAccessJson (TRI_aql_context_t* const context,
                                      const TRI_json_t* const json,
                                      TRI_string_buffer_t* const buffer) {
  TRI_StringifyJson(buffer, json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a range access
////////////////////////////////////////////////////////////////////////////////

static void StringifyFieldAccessRange (TRI_aql_context_t* const context,
                                       const TRI_aql_range_t* const range,
                                       TRI_string_buffer_t* const buffer) {
  TRI_AppendStringStringBuffer(buffer, RangeName(range->_type));
  TRI_AppendCharStringBuffer(buffer, ':');
  StringifyFieldAccessJson(context, range->_value, buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a reference access
////////////////////////////////////////////////////////////////////////////////

static void StringifyFieldAccessReference (TRI_aql_context_t* const context,
                                           const TRI_aql_field_access_t* const fieldAccess,
                                           TRI_string_buffer_t* const buffer) {
  TRI_AppendStringStringBuffer(buffer, TRI_NodeNameAql(fieldAccess->_value._reference._operator));
  TRI_AppendCharStringBuffer(buffer, ':');

  if (fieldAccess->_value._reference._type == TRI_AQL_REFERENCE_VARIABLE) {
    TRI_AppendStringStringBuffer(buffer, "variable:");
    // append variable name
    TRI_AppendStringStringBuffer(buffer, fieldAccess->_value._reference._ref._name);
  }
  else if (fieldAccess->_value._reference._type == TRI_AQL_REFERENCE_ATTRIBUTE_ACCESS) {
    TRI_aql_attribute_name_t* attributeName;

    attributeName = GetAttributeName(context, fieldAccess->_value._reference._ref._node);
    if (attributeName == NULL) {
      // out of memory
      return;
    }

    // append node info
    TRI_AppendStringStringBuffer(buffer, "attribute:");
    TRI_AppendStringStringBuffer(buffer, attributeName->_name._buffer);

    TRI_DestroyStringBuffer(&attributeName->_name);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, attributeName);
  }
  else {
    assert(false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringify a field access
////////////////////////////////////////////////////////////////////////////////

static char* StringifyFieldAccess (TRI_aql_context_t* const context,
                                   const TRI_aql_field_access_t* const fieldAccess) {
  TRI_string_buffer_t buffer;
  char* result;

  TRI_InitStringBuffer(&buffer, TRI_UNKNOWN_MEM_ZONE);
  if (buffer._buffer == NULL) {
    // out of memory
    return NULL;
  }

  TRI_AppendStringStringBuffer(&buffer, TypeName(fieldAccess->_type));

  switch (fieldAccess->_type) {
    case TRI_AQL_ACCESS_EXACT:
    case TRI_AQL_ACCESS_LIST:
      TRI_AppendCharStringBuffer(&buffer, ':');
      StringifyFieldAccessJson(context, fieldAccess->_value._value, &buffer);
      break;

    case TRI_AQL_ACCESS_RANGE_SINGLE:
      TRI_AppendCharStringBuffer(&buffer, ':');
      StringifyFieldAccessRange(context, &fieldAccess->_value._singleRange, &buffer);
      break;

    case TRI_AQL_ACCESS_RANGE_DOUBLE:
      TRI_AppendCharStringBuffer(&buffer, ':');
      StringifyFieldAccessRange(context, &fieldAccess->_value._between._lower, &buffer);
      TRI_AppendCharStringBuffer(&buffer, ':');
      StringifyFieldAccessRange(context, &fieldAccess->_value._between._upper, &buffer);
      break;

    case TRI_AQL_ACCESS_REFERENCE:
      TRI_AppendCharStringBuffer(&buffer, ':');
      StringifyFieldAccessReference(context, fieldAccess, &buffer);
      break;

    case TRI_AQL_ACCESS_IMPOSSIBLE:
    case TRI_AQL_ACCESS_ALL:
    default: {
      // nothing to do here
    }
  }

  result = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, buffer._buffer);
  TRI_DestroyStringBuffer(&buffer);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an or element by field name
////////////////////////////////////////////////////////////////////////////////

static or_element_t* FindOrElement (const TRI_vector_pointer_t* const vector,
                                    const char* const name) {
  size_t i, n;

  TRI_ASSERT_DEBUG(vector != NULL);
  TRI_ASSERT_DEBUG(name != NULL);

  n = vector->_length;
  for (i = 0; i < n; ++i) {
    or_element_t* current;

    current = (or_element_t*) TRI_AtVectorPointer(vector, i);

    TRI_ASSERT_DEBUG(current != NULL);

    if (TRI_EqualString(name, current->_name)) {
      return current;
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert all field accesses from a vector into an OR aggregation vector
////////////////////////////////////////////////////////////////////////////////

static bool InsertOrs (TRI_aql_context_t* const context,
                       const TRI_vector_pointer_t* const source,
                       TRI_vector_pointer_t* const dest) {
  size_t i, n;

  if (dest == NULL) {
    return true;
  }

  n = source->_length;
  for (i = 0; i < n; ++i) {
    or_element_t* orElement;
    TRI_aql_field_access_t* fieldAccess;

    fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(source, i);

    TRI_ASSERT_DEBUG(fieldAccess != NULL);
    TRI_ASSERT_DEBUG(fieldAccess->_fullName != NULL);

    orElement = FindOrElement(dest, fieldAccess->_fullName);
    if (orElement == NULL) {
      // element not found. create a new one
      orElement = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(or_element_t), false);
      if (orElement == NULL) {
        // out of memory
        return false;
      }

      orElement->_count = 1;
      orElement->_condition = StringifyFieldAccess(context, fieldAccess);

      if (orElement->_condition == NULL) {
        // out of memory
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, orElement);

        return false;
      }

      orElement->_name = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, fieldAccess->_fullName);
      if (orElement->_name == NULL) {
        // out of memory
        TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, orElement->_condition);
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, orElement);

        return false;
      }

      TRI_PushBackVectorPointer(dest, (void*) orElement);
    }
    else {
      // compare previous with current condition
      char* condition = StringifyFieldAccess(context, fieldAccess);

      if (condition == NULL) {
        // out of memory
        return false;
      }

      if (TRI_EqualString(condition, orElement->_condition)) {
        // conditions are identical, match!
        orElement->_count++;
      }

      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, condition);
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the length of the variable name for a field access struct
////////////////////////////////////////////////////////////////////////////////

static void SetNameLength (TRI_aql_field_access_t* const fieldAccess) {
  char* dotPosition;

  TRI_ASSERT_DEBUG(fieldAccess != NULL);
  TRI_ASSERT_DEBUG(fieldAccess->_fullName != NULL);

  dotPosition = strchr(fieldAccess->_fullName, '.');
  if (dotPosition == NULL) {
    // field does not contain .
    fieldAccess->_variableNameLength = strlen(fieldAccess->_fullName);
  }
  else {
    fieldAccess->_variableNameLength = (size_t) (dotPosition - fieldAccess->_fullName);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free access member data, but do not free the access struct itself
////////////////////////////////////////////////////////////////////////////////

static void FreeAccessMembers (TRI_aql_field_access_t* const fieldAccess) {
  TRI_ASSERT_DEBUG(fieldAccess != NULL);

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

    case TRI_AQL_ACCESS_REFERENCE:
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

  fieldAccess->_fullName = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, fullName);
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
/// @brief check if two references refer to the same variable or attribute
////////////////////////////////////////////////////////////////////////////////

bool IsSameReference (const TRI_aql_field_access_t* const lhs,
                      const TRI_aql_field_access_t* const rhs) {
  TRI_ASSERT_DEBUG(lhs->_type == TRI_AQL_ACCESS_REFERENCE);
  TRI_ASSERT_DEBUG(rhs->_type == TRI_AQL_ACCESS_REFERENCE);

  if (lhs->_value._reference._type == TRI_AQL_REFERENCE_VARIABLE &&
      rhs->_value._reference._type == TRI_AQL_REFERENCE_VARIABLE) {

    return TRI_EqualString(lhs->_value._reference._ref._name, rhs->_value._reference._ref._name);
  }

  if (lhs->_value._reference._type == TRI_AQL_REFERENCE_ATTRIBUTE_ACCESS &&
      rhs->_value._reference._type == TRI_AQL_REFERENCE_ATTRIBUTE_ACCESS) {
    return TRI_EqualString(lhs->_fullName, rhs->_fullName);
  }

  return false;
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
  TRI_ASSERT_DEBUG(lhs->_type == TRI_AQL_ACCESS_IMPOSSIBLE);
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
  TRI_ASSERT_DEBUG(lhs->_type == TRI_AQL_ACCESS_ALL);
  TRI_FreeAccessAql(lhs);

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
  TRI_ASSERT_DEBUG(lhs->_type == TRI_AQL_ACCESS_EXACT);

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

  if (rhs->_type == TRI_AQL_ACCESS_REFERENCE) {
    // for simplicity, always return the const access
    TRI_FreeAccessAql(rhs);
    return lhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_ALL) {
    TRI_FreeAccessAql(rhs);
    return lhs;
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
  TRI_ASSERT_DEBUG(lhs->_type == TRI_AQL_ACCESS_LIST);

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

  if (rhs->_type == TRI_AQL_ACCESS_REFERENCE) {
    // for simplicity, always return the const access
    TRI_FreeAccessAql(rhs);
    return lhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_ALL) {
    TRI_FreeAccessAql(rhs);
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
  TRI_ASSERT_DEBUG(lhs->_type == TRI_AQL_ACCESS_RANGE_SINGLE);

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

        lhs->_value._singleRange._type = TRI_AQL_RANGE_LOWER_INCLUDED;
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

        lhs->_value._singleRange._type = TRI_AQL_RANGE_UPPER_INCLUDED;
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

  if (rhs->_type == TRI_AQL_ACCESS_REFERENCE) {
    // for simplicity, always return the const access
    TRI_FreeAccessAql(rhs);
    return lhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_ALL) {
    TRI_FreeAccessAql(rhs);
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

  TRI_ASSERT_DEBUG(lhs->_type == TRI_AQL_ACCESS_RANGE_DOUBLE);

  if (rhs->_type == TRI_AQL_ACCESS_RANGE_DOUBLE) {
    int compareResult;

    // check lower bound
    compareResult = TRI_CompareValuesJson(lhs->_value._between._lower._value, rhs->_value._between._lower._value);
    if (compareResult > 0) {
      // we'll patch lhs with the value of rhs
      lhs->_value._between._lower._type = rhs->_value._between._lower._type;
      lhs->_value._between._lower._value = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, rhs->_value._between._lower._value);
      if (lhs->_value._between._lower._value == NULL) {
        // OOM
        TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      }
    }
    else if (compareResult == 0 && rhs->_value._between._lower._type == TRI_AQL_RANGE_LOWER_EXCLUDED) {
      // lhs and rhs value is the same, but if rhs is a > range, we can safely use it
      lhs->_value._between._lower._type = rhs->_value._between._lower._type;
    }

    // check upper bound
    compareResult = TRI_CompareValuesJson(lhs->_value._between._upper._value, rhs->_value._between._upper._value);
    if (compareResult < 0) {
      // we'll patch lhs with the value of rhs
      lhs->_value._between._upper._type = rhs->_value._between._upper._type;
      lhs->_value._between._upper._value = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, rhs->_value._between._upper._value);
      if (lhs->_value._between._upper._value == NULL) {
        // OOM
        TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      }
    }
    else if (compareResult == 0 && rhs->_value._between._upper._type == TRI_AQL_RANGE_UPPER_EXCLUDED) {
      // lhs and rhs value is the same, but if rhs is a < range, we can safely use it
      lhs->_value._between._upper._type = rhs->_value._between._upper._type;
    }

    // we patched lhs so we'll always free rhs
    TRI_FreeAccessAql(rhs);
    return lhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_REFERENCE) {
    // for simplicity, always return the const access
    TRI_FreeAccessAql(rhs);
    return lhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_ALL) {
    TRI_FreeAccessAql(rhs);
    return lhs;
  }

  assert(false);
  /* this should never be called. let's see if this assumption is true or not */

  return lhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two access structures using a logical AND
///
/// left hand operand is a reference
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeAndReference (TRI_aql_context_t* const context,
                                                  TRI_aql_field_access_t* lhs,
                                                  TRI_aql_field_access_t* rhs) {
  TRI_ASSERT_DEBUG(lhs->_type == TRI_AQL_ACCESS_REFERENCE);

  if (rhs->_type == TRI_AQL_ACCESS_REFERENCE) {
    TRI_aql_node_type_e lhsType = lhs->_value._reference._operator;
    TRI_aql_node_type_e rhsType = rhs->_value._reference._operator;
    bool isSameAttribute = IsSameReference(lhs, rhs);
    bool possible;

    if (!isSameAttribute) {
      // different attribute names are referred to. we can return either
      TRI_FreeAccessAql(rhs);
      return lhs;
    }

    // both references refer to the same variable, now compare their operators
    assert(isSameAttribute);

    if (lhsType == rhsType) {
      // same operator, i.e. the two references are absolutely identical
      TRI_FreeAccessAql(rhs);

      return lhs;
    }

    if (lhsType == TRI_AQL_NODE_OPERATOR_BINARY_LT && rhsType == TRI_AQL_NODE_OPERATOR_BINARY_LE) {
      // < && <=, merge to <
      TRI_FreeAccessAql(rhs);

      return lhs;
    }
    if (rhsType == TRI_AQL_NODE_OPERATOR_BINARY_LT && lhsType == TRI_AQL_NODE_OPERATOR_BINARY_LE) {
      // <= && <, merge to <
      TRI_FreeAccessAql(lhs);

      return rhs;
    }

    possible = true;

    if (lhsType == TRI_AQL_NODE_OPERATOR_BINARY_EQ &&
        (rhsType == TRI_AQL_NODE_OPERATOR_BINARY_LT ||
         rhsType == TRI_AQL_NODE_OPERATOR_BINARY_GT)) {
      // lhs == ref && (lhs < ref || lhs > ref)
      possible = false;
    }
    else if (rhsType == TRI_AQL_NODE_OPERATOR_BINARY_EQ &&
             (lhsType == TRI_AQL_NODE_OPERATOR_BINARY_LT ||
              lhsType == TRI_AQL_NODE_OPERATOR_BINARY_GT)) {
      // (lhs < ref || lhs > ref) && lhs == ref
      possible = false;
    }
    else if (lhsType == TRI_AQL_NODE_OPERATOR_BINARY_LT &&
        (rhsType == TRI_AQL_NODE_OPERATOR_BINARY_GE || rhsType == TRI_AQL_NODE_OPERATOR_BINARY_GT)) {
      // lhs < ref && (lhs >= ref || lhs > ref) => impossible
      possible = false;
    }
    else if (lhsType == TRI_AQL_NODE_OPERATOR_BINARY_LE &&
             rhsType == TRI_AQL_NODE_OPERATOR_BINARY_GT) {
      // lhs <= ref && lhs > ref => impossible
      possible = false;
    }
    else if (lhsType == TRI_AQL_NODE_OPERATOR_BINARY_GT &&
             (rhsType == TRI_AQL_NODE_OPERATOR_BINARY_LE || rhsType == TRI_AQL_NODE_OPERATOR_BINARY_LT)) {
      // lhs > ref && (lhs <= ref || lhs < ref) => impossible
      possible = false;
    }
    else if (lhsType == TRI_AQL_NODE_OPERATOR_BINARY_GE &&
             rhsType == TRI_AQL_NODE_OPERATOR_BINARY_LT) {
      // lhs >= ref && lhs < ref => impossible
      possible = false;
    }

    if (!possible) {
      // return the impossible range
      TRI_FreeAccessAql(rhs);
      FreeAccessMembers(lhs);
      lhs->_type = TRI_AQL_ACCESS_IMPOSSIBLE;

      return lhs;
    }

    // everything else results in lhs
    TRI_FreeAccessAql(rhs);
    return lhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_ALL) {
    TRI_FreeAccessAql(rhs);
    return lhs;
  }

  assert(false);
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
  TRI_ASSERT_DEBUG(lhs->_type == TRI_AQL_ACCESS_IMPOSSIBLE);
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
  TRI_ASSERT_DEBUG(lhs->_type == TRI_AQL_ACCESS_ALL);
  TRI_FreeAccessAql(rhs);

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
  TRI_ASSERT_DEBUG(lhs->_type == TRI_AQL_ACCESS_EXACT);

  if (rhs->_type == TRI_AQL_ACCESS_EXACT) {
    TRI_json_t* result;

    // check if values are identical
    if (TRI_CheckSameValueJson(lhs->_value._value, rhs->_value._value)) {
      // lhs and rhs values are identical, return lhs
      TRI_FreeAccessAql(rhs);

      return lhs;
    }

    // make a list with both values
    result = TRI_CreateList2Json(TRI_UNKNOWN_MEM_ZONE, 2);
    if (result == NULL) {
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

  if (rhs->_type == TRI_AQL_ACCESS_REFERENCE) {
    // reference cannot be ORed with anything else
    TRI_FreeAccessAql(rhs);
    FreeAccessMembers(lhs);
    lhs->_type = TRI_AQL_ACCESS_ALL;

    return lhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_ALL) {
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
  TRI_ASSERT_DEBUG(lhs->_type == TRI_AQL_ACCESS_LIST);

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
  TRI_ASSERT_DEBUG(lhs->_type == TRI_AQL_ACCESS_RANGE_SINGLE);

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
  TRI_ASSERT_DEBUG(lhs->_type == TRI_AQL_ACCESS_RANGE_DOUBLE);

  if (rhs->_type == TRI_AQL_ACCESS_RANGE_DOUBLE) {
    int compareResult;

    // check lower bound
    compareResult = TRI_CompareValuesJson(lhs->_value._between._lower._value, rhs->_value._between._lower._value);
    if (compareResult < 0) {
      // we'll patch lhs with the value of rhs
      lhs->_value._between._lower._type = rhs->_value._between._lower._type;
      lhs->_value._between._lower._value = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, rhs->_value._between._lower._value);
      if (lhs->_value._between._lower._value == NULL) {
        // OOM
        TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      }
    }
    else if (compareResult == 0 && rhs->_value._between._lower._type == TRI_AQL_RANGE_LOWER_INCLUDED) {
      // lhs and rhs value is the same, but if rhs is a >= range, we must use it
      lhs->_value._between._lower._type = rhs->_value._between._lower._type;
    }

    // check upper bound
    compareResult = TRI_CompareValuesJson(lhs->_value._between._upper._value, rhs->_value._between._upper._value);
    if (compareResult > 0) {
      // we'll patch lhs with the value of rhs
      lhs->_value._between._upper._type = rhs->_value._between._upper._type;
      lhs->_value._between._upper._value = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, rhs->_value._between._upper._value);
      if (lhs->_value._between._upper._value == NULL) {
        // OOM
        TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      }
    }
    else if (compareResult == 0 && rhs->_value._between._upper._type == TRI_AQL_RANGE_UPPER_INCLUDED) {
      // lhs and rhs value is the same, but if rhs is a <= range, we must use it
      lhs->_value._between._upper._type = rhs->_value._between._upper._type;
    }

    // we patched lhs so we'll always free rhs
    TRI_FreeAccessAql(rhs);
    return lhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_REFERENCE) {
    // reference cannot be ORed with anything else
    TRI_FreeAccessAql(rhs);
    FreeAccessMembers(lhs);
    lhs->_type = TRI_AQL_ACCESS_ALL;

    return lhs;
  }

  if (rhs->_type == TRI_AQL_ACCESS_ALL) {
    TRI_FreeAccessAql(lhs);
    return rhs;
  }

  assert(false);
  /* this should never be called. let's see if this assumption is true or not */

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
  TRI_ASSERT_DEBUG(lhs->_type == TRI_AQL_ACCESS_REFERENCE);

  if (rhs->_type == TRI_AQL_ACCESS_REFERENCE) {
    TRI_aql_node_type_e lhsType = lhs->_value._reference._operator;
    TRI_aql_node_type_e rhsType = rhs->_value._reference._operator;
    bool isSameAttribute = IsSameReference(lhs, rhs);

    if (!isSameAttribute) {
      // references refer to different attributes
      TRI_FreeAccessAql(rhs);
      FreeAccessMembers(lhs);
      lhs->_type = TRI_AQL_ACCESS_ALL;

      return lhs;
    }

    // both references refer to the same thing, now compare their operators
    assert(isSameAttribute);

    if (lhsType == rhsType) {
      // same operator, i.e. the two references are identical
      TRI_FreeAccessAql(rhs);

      return lhs;
    }
    else if (lhsType == TRI_AQL_NODE_OPERATOR_BINARY_LT && rhsType == TRI_AQL_NODE_OPERATOR_BINARY_LE) {
      // < || <=, merge to <=
      TRI_FreeAccessAql(lhs);

      return rhs;
    }
    else if (lhsType == TRI_AQL_NODE_OPERATOR_BINARY_LE && rhsType == TRI_AQL_NODE_OPERATOR_BINARY_LT) {
      // <= || <, merge to <=
      TRI_FreeAccessAql(rhs);

      return lhs;
    }
    else if (lhsType == TRI_AQL_NODE_OPERATOR_BINARY_GT && rhsType == TRI_AQL_NODE_OPERATOR_BINARY_GE) {
      // > || >=, merge to >=
      TRI_FreeAccessAql(lhs);

      return rhs;
    }
    else if (lhsType == TRI_AQL_NODE_OPERATOR_BINARY_GE && rhsType == TRI_AQL_NODE_OPERATOR_BINARY_GT) {
      // >= || >, merge to >=
      TRI_FreeAccessAql(rhs);

      return lhs;
    }

    // fall-through
  }


  // for everything else, we have to use the ALL range unfortunately
  TRI_FreeAccessAql(rhs);
  FreeAccessMembers(lhs);
  lhs->_type = TRI_AQL_ACCESS_ALL;

  return lhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two attribute access structures using logical AND
////////////////////////////////////////////////////////////////////////////////

static TRI_aql_field_access_t* MergeAttributeAccessAnd (TRI_aql_context_t* const context,
                                                        TRI_aql_field_access_t* lhs,
                                                        TRI_aql_field_access_t* rhs) {
  TRI_ASSERT_DEBUG(context != NULL);
  TRI_ASSERT_DEBUG(lhs != NULL);
  TRI_ASSERT_DEBUG(rhs != NULL);
  TRI_ASSERT_DEBUG(lhs->_fullName != NULL);
  TRI_ASSERT_DEBUG(rhs->_fullName != NULL);

  if (lhs->_type > rhs->_type) {
    // swap operands so they are always sorted
    TRI_aql_field_access_t* tmp = lhs;
    lhs = rhs;
    rhs = tmp;
  }

  TRI_ASSERT_DEBUG(lhs->_type <= rhs->_type);

  switch (lhs->_type) {
    case TRI_AQL_ACCESS_IMPOSSIBLE:
      return MergeAndImpossible(context, lhs, rhs);
    case TRI_AQL_ACCESS_EXACT:
      return MergeAndExact(context, lhs, rhs);
    case TRI_AQL_ACCESS_LIST:
      return MergeAndList(context, lhs, rhs);
    case TRI_AQL_ACCESS_RANGE_SINGLE:
      return MergeAndRangeSingle(context, lhs, rhs);
    case TRI_AQL_ACCESS_RANGE_DOUBLE:
      return MergeAndRangeDouble(context, lhs, rhs);
    case TRI_AQL_ACCESS_REFERENCE:
      return MergeAndReference(context, lhs, rhs);
    case TRI_AQL_ACCESS_ALL:
      return MergeAndAll(context, lhs, rhs);
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
  TRI_ASSERT_DEBUG(context != NULL);
  TRI_ASSERT_DEBUG(lhs != NULL);
  TRI_ASSERT_DEBUG(rhs != NULL);
  TRI_ASSERT_DEBUG(lhs->_fullName != NULL);
  TRI_ASSERT_DEBUG(rhs->_fullName != NULL);

  if (lhs->_type > rhs->_type) {
    // swap operands so they are always sorted
    TRI_aql_field_access_t* tmp = lhs;
    lhs = rhs;
    rhs = tmp;
  }

  TRI_ASSERT_DEBUG(lhs->_type <= rhs->_type);

  switch (lhs->_type) {
    case TRI_AQL_ACCESS_IMPOSSIBLE:
      return MergeOrImpossible(context, lhs, rhs);
    case TRI_AQL_ACCESS_EXACT:
      return MergeOrExact(context, lhs, rhs);
    case TRI_AQL_ACCESS_LIST:
      return MergeOrList(context, lhs, rhs);
    case TRI_AQL_ACCESS_RANGE_SINGLE:
      return MergeOrRangeSingle(context, lhs, rhs);
    case TRI_AQL_ACCESS_RANGE_DOUBLE:
      return MergeOrRangeDouble(context, lhs, rhs);
    case TRI_AQL_ACCESS_REFERENCE:
      return MergeOrReference(context, lhs, rhs);
    case TRI_AQL_ACCESS_ALL:
      return MergeOrAll(context, lhs, rhs);
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

  TRI_ASSERT_DEBUG(context != NULL);

  result = (TRI_vector_pointer_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vector_pointer_t), false);

  if (result == NULL) {
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

  TRI_ASSERT_DEBUG(context != NULL);

  if (fieldAccess == NULL) {
    return NULL;
  }

  vector = CreateEmptyVector(context);
  if (vector == NULL) {
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

  if (fieldAccesses == NULL) {
    return NULL;
  }

  n = fieldAccesses->_length;
  for (i = 0; i < n; ++i) {
    // turn all field access values into an all items access
    TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(fieldAccesses, i);

    TRI_ASSERT_DEBUG(fieldAccess != NULL);
    TRI_ASSERT_DEBUG(fieldAccess->_fullName != NULL);

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

  TRI_ASSERT_DEBUG(result != NULL);
  TRI_ASSERT_DEBUG(source != NULL);

  n = source->_length;
  for (i = 0; i < n; ++i) {
    TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(source, i);
    size_t j, len;
    bool found = false;

    TRI_ASSERT_DEBUG(fieldAccess != NULL);
    TRI_ASSERT_DEBUG(fieldAccess->_fullName != NULL);

    // check if element is in result vector already
    len = result->_length;
    for (j = 0; j < len; ++j) {
      TRI_aql_field_access_t* compareAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(result, j);

      assert(compareAccess);
      assert(compareAccess->_fullName);

      // check if the current buffer element is the one we want to update
      if (TRI_EqualString(fieldAccess->_fullName, compareAccess->_fullName)) {
        // found the element
        if (mergeType == TRI_AQL_NODE_OPERATOR_BINARY_AND) {
          // update the existing element in place
          result->_buffer[j] = MergeAttributeAccessAnd(context, fieldAccess, compareAccess);
        }
        else {
          // update the existing element in place
          result->_buffer[j] = MergeAttributeAccessOr(context, fieldAccess, compareAccess);
        }

        found = true;
        break;
      }
    }

    if (! found) {
      // element not found, now add it to the list of restrictions
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

  TRI_ASSERT_DEBUG(result != NULL);
  TRI_ASSERT_DEBUG(source != NULL);

  n = source->_length;
  for (i = 0; i < n; ++i) {
    TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(source, i);
    TRI_aql_field_access_t* copy;
    
    TRI_ASSERT_DEBUG(fieldAccess != NULL);
    TRI_ASSERT_DEBUG(fieldAccess->_fullName != NULL);

    copy = TRI_CloneAccessAql(context, fieldAccess);

    if (copy == NULL) {
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
  TRI_vector_pointer_t* orElements;

  TRI_ASSERT_DEBUG(context != NULL);
  TRI_ASSERT_DEBUG(mergeType == TRI_AQL_NODE_OPERATOR_BINARY_AND || mergeType == TRI_AQL_NODE_OPERATOR_BINARY_OR);

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
  if (result == NULL) {
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

  orElements = NULL;

  if (mergeType == TRI_AQL_NODE_OPERATOR_BINARY_OR &&  lhs && rhs) {
    // this is an OR merge
    // we need to check which elements are contained in just one of the vectors and
    // remove them. if we don't do this, we would probably restrict the query too much
    orElements = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vector_pointer_t), false);
    if (orElements == NULL) {
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

      return NULL;
    }

    TRI_InitVectorPointer(orElements, TRI_UNKNOWN_MEM_ZONE);
  }


  if (inheritedRestrictions) {
    // insert a copy of all restrictions first
    InsertVector(context, result, inheritedRestrictions);
  }

  if (lhs) {
    // this is a no-op if there is no orElements vector
    InsertOrs(context, lhs, orElements);

    // copy elements from lhs into result vector
    MergeVector(context, mergeType, result, lhs);
    TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, lhs);
  }

  if (rhs) {
    // this is a no-op if there is no orElements vector
    InsertOrs(context, rhs, orElements);

    // copy elements from rhs into result vector
    MergeVector(context, mergeType, result, rhs);
    TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, rhs);
  }


  if (orElements) {
    // this is an OR merge
    // we need to check which elements are contained in just one of the vectors and
    // remove them. if we don't do this, we would probably restrict the query too much
    size_t i;

    for (i = 0; i < result->_length; ++i) {
      TRI_aql_field_access_t* fieldAccess;
      or_element_t* orElement;

      fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(result, i);
      TRI_ASSERT_DEBUG(fieldAccess != NULL);

      if (fieldAccess->_type == TRI_AQL_ACCESS_ALL) {
        continue;
      }

      orElement = FindOrElement(orElements, fieldAccess->_fullName);
      if (orElement == NULL || orElement->_count < 2) {
        // must make the element an all access
        FreeAccessMembers(fieldAccess);
        fieldAccess->_type = TRI_AQL_ACCESS_ALL;
      }
    }

    // free all orElements
    for (i = 0; i < orElements->_length; ++i) {
      or_element_t* orElement;

      orElement = (or_element_t*) TRI_AtVectorPointer(orElements, i);
      TRI_ASSERT_DEBUG(orElement != NULL);

      // free members
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, orElement->_name);
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, orElement->_condition);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, orElement);
    }

    TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, orElements);
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

  TRI_ASSERT_DEBUG(context != NULL);
  TRI_ASSERT_DEBUG(field != NULL);
  TRI_ASSERT_DEBUG(field->_name._buffer != NULL);
  TRI_ASSERT_DEBUG(node != NULL);

  fieldAccess = (TRI_aql_field_access_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_field_access_t), false);
  if (fieldAccess == NULL) {
    // OOM
    TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);

    return NULL;
  }

  fieldAccess->_fullName = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, field->_name._buffer);
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

  if (node->_type == TRI_AQL_NODE_REFERENCE ||
      node->_type == TRI_AQL_NODE_ATTRIBUTE_ACCESS) {
    // create the reference access
    fieldAccess->_type = TRI_AQL_ACCESS_REFERENCE;
    fieldAccess->_value._reference._operator = operator;
    if (node->_type == TRI_AQL_NODE_REFERENCE) {
      fieldAccess->_value._reference._type = TRI_AQL_REFERENCE_VARIABLE;
      fieldAccess->_value._reference._ref._name = TRI_AQL_NODE_STRING(node);
    }
    else if (node->_type == TRI_AQL_NODE_ATTRIBUTE_ACCESS) {
      fieldAccess->_value._reference._type = TRI_AQL_REFERENCE_ATTRIBUTE_ACCESS;
      fieldAccess->_value._reference._ref._node = (TRI_aql_node_t*) node;
    }

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

    TRI_ASSERT_DEBUG(value->_type == TRI_JSON_LIST);

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

  TRI_ASSERT_DEBUG(context != NULL);
  TRI_ASSERT_DEBUG(node != NULL);

  if (field == NULL || ! field->_name._buffer) {
    // this is ok if the node type is not supported
    return NULL;
  }

  fieldAccess = CreateAccessForNode(context, field, operator, node);
  if (fieldAccess == NULL) {
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
  TRI_ASSERT_DEBUG(context != NULL);
  TRI_ASSERT_DEBUG(node != NULL);

  if (node->_type == TRI_AQL_NODE_ATTRIBUTE_ACCESS) {
    TRI_aql_attribute_name_t* field = GetAttributeName(context, TRI_AQL_NODE_MEMBER(node, 0));

    if (field == NULL) {
      return NULL;
    }

    TRI_AppendCharStringBuffer(&field->_name, '.');
    TRI_AppendStringStringBuffer(&field->_name, TRI_AQL_NODE_STRING(node));

    return field;
  }
  else if (node->_type == TRI_AQL_NODE_REFERENCE) {
    TRI_aql_attribute_name_t* field;

    field = (TRI_aql_attribute_name_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_aql_attribute_name_t), false);

    if (field == NULL) {
      // OOM
      TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      return NULL;
    }

    field->_variable = TRI_AQL_NODE_STRING(node);
    TRI_InitStringBuffer(&field->_name, TRI_UNKNOWN_MEM_ZONE);
    TRI_AppendStringStringBuffer(&field->_name, TRI_AQL_NODE_STRING(node));

    return field;
  }
  else if (node->_type == TRI_AQL_NODE_FCALL) {
    TRI_aql_function_t* function = (TRI_aql_function_t*) TRI_AQL_NODE_DATA(node);
    TRI_aql_node_t* args = TRI_AQL_NODE_MEMBER(node, 0);
    TRI_aql_attribute_name_t* field;

    // if we have anything but 1 function call argument, we cannot optimise this
    if (args->_members._length != 1) {
      return NULL;
    }

    field = GetAttributeName(context, TRI_AQL_NODE_MEMBER(args, 0));

    if (field == NULL) {
      return NULL;
    }

    // name of generated attribute is XXX.FUNC(), e.g. for LENGTH(users.friends) => users.friends.LENGTH()
    TRI_AppendCharStringBuffer(&field->_name, '.');
    TRI_AppendStringStringBuffer(&field->_name, function->_externalName);
    TRI_AppendStringStringBuffer(&field->_name, "()");

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
  TRI_ASSERT_DEBUG(context != NULL);
  TRI_ASSERT_DEBUG(node != NULL);

  if (node->_type == TRI_AQL_NODE_OPERATOR_UNARY_NOT) {
    TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);

    TRI_ASSERT_DEBUG(lhs != NULL);

    // can ignore inherited restrictions here
    return MakeAllVector(context, ProcessNode(context, lhs, changed, NULL));
  }

  if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_OR ||
      node->_type == TRI_AQL_NODE_OPERATOR_BINARY_AND) {
    TRI_aql_node_t* lhs = TRI_AQL_NODE_MEMBER(node, 0);
    TRI_aql_node_t* rhs = TRI_AQL_NODE_MEMBER(node, 1);
    TRI_vector_pointer_t* result;

    TRI_ASSERT_DEBUG(lhs != NULL);
    TRI_ASSERT_DEBUG(rhs != NULL);

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
    TRI_vector_pointer_t* previous;
    TRI_aql_attribute_name_t* field;
    TRI_aql_node_t* node1;
    TRI_aql_node_t* node2;
    TRI_aql_node_type_e operator;
    bool useBoth;

    if (node->_type == TRI_AQL_NODE_OPERATOR_BINARY_IN && rhs->_type != TRI_AQL_NODE_LIST) {
      // in operator is special. if right operand is not a list, we must abort here
      return NULL;
    }

    useBoth = false;

    if ((lhs->_type == TRI_AQL_NODE_REFERENCE || lhs->_type == TRI_AQL_NODE_ATTRIBUTE_ACCESS || lhs->_type == TRI_AQL_NODE_FCALL) &&
        (TRI_IsConstantValueNodeAql(rhs) || rhs->_type == TRI_AQL_NODE_REFERENCE || rhs->_type == TRI_AQL_NODE_ATTRIBUTE_ACCESS || rhs->_type == TRI_AQL_NODE_FCALL)) {
      // collection.attribute|reference|fcall operator const value|reference|attribute access|fcall
      node1 = lhs;
      node2 = rhs;
      operator = node->_type;

      if (rhs->_type == TRI_AQL_NODE_REFERENCE || rhs->_type == TRI_AQL_NODE_ATTRIBUTE_ACCESS || rhs->_type == TRI_AQL_NODE_FCALL) {
        // expression of type reference|attribute access|fcall operator reference|attribute access|fcall
        useBoth = true;
      }
    }
    else if ((rhs->_type == TRI_AQL_NODE_REFERENCE || rhs->_type == TRI_AQL_NODE_ATTRIBUTE_ACCESS || rhs->_type == TRI_AQL_NODE_FCALL) &&
             (TRI_IsConstantValueNodeAql(lhs) || lhs->_type == TRI_AQL_NODE_REFERENCE || lhs->_type == TRI_AQL_NODE_ATTRIBUTE_ACCESS || lhs->_type == TRI_AQL_NODE_FCALL)) {
      // const value|reference|attribute|fcall access operator collection.attribute|reference|fcall
      node1 = rhs;
      node2 = lhs;
      operator = TRI_ReverseOperatorRelationalAql(node->_type);
    
      TRI_ASSERT_DEBUG(operator != TRI_AQL_NODE_NOP);

      if (lhs->_type == TRI_AQL_NODE_REFERENCE || lhs->_type == TRI_AQL_NODE_ATTRIBUTE_ACCESS || lhs->_type == TRI_AQL_NODE_FCALL) {
        // expression of type reference|attribute access|fcall operator reference|attribute access|fcall
        useBoth = true;
      }
    }
    else {
      return NULL;
    }

    if (node2->_type != TRI_AQL_NODE_VALUE &&
        node2->_type != TRI_AQL_NODE_LIST &&
        node2->_type != TRI_AQL_NODE_ARRAY &&
        node2->_type != TRI_AQL_NODE_REFERENCE &&
        node2->_type != TRI_AQL_NODE_ATTRIBUTE_ACCESS) {
      // only the above types are supported
      return NULL;
    }

    previous = NULL;

again:
    // we'll get back here for expressions of type a.x == b.y (where both sides are references)
    field = GetAttributeName(context, node1);

    if (field && node2->_type != TRI_AQL_NODE_FCALL) {
      TRI_aql_field_access_t* attributeAccess = GetAttributeAccess(context, field, operator, node2);
      TRI_vector_pointer_t* result;

      TRI_DestroyStringBuffer(&field->_name);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, field);

      result = MergeVectors(context,
                            TRI_AQL_NODE_OPERATOR_BINARY_AND,
                            Vectorize(context, attributeAccess),
                            previous,
                            inheritedRestrictions);

      if (result == NULL) {
        TRI_SetErrorContextAql(context, TRI_ERROR_OUT_OF_MEMORY, NULL);
      }
      else {
        if (TRI_ContainsImpossibleAql(result)) {
          // inject a dummy false == true node into the tree if the condition is always false
          node->_type = TRI_AQL_NODE_OPERATOR_BINARY_EQ;
          node->_members._buffer[0] = TRI_CreateNodeValueBoolAql(context, false);
          node->_members._buffer[1] = TRI_CreateNodeValueBoolAql(context, true);

          // set changed marker
          *changed = true;
        }
      }

      if (useBoth) {
        // in this situation, we have an expression of type a.x == b.y
        // we'll have to process both sides of the expression
        TRI_aql_node_t* tempNode;

        // swap node1 and node2
        tempNode = node1;
        node1 = node2;
        node2 = tempNode;

        operator = TRI_ReverseOperatorRelationalAql(node->_type);

        // and try again
        previous = result;
        useBoth = false;
        goto again;
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
/// @brief dump an AQL access type
////////////////////////////////////////////////////////////////////////////////

void TRI_DumpAccessAql (const TRI_aql_field_access_t* const fieldAccess) {
  LOG_INFO("info for field access. field: '%s', type: %s",
           fieldAccess->_fullName,
           TypeName(fieldAccess->_type));
}

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

  if (fieldAccesses == NULL) {
    return false;
  }

  n = fieldAccesses->_length;
  for (i = 0; i < n; ++i) {
    TRI_aql_field_access_t* fieldAccess = (TRI_aql_field_access_t*) TRI_AtVectorPointer(fieldAccesses, i);

    TRI_ASSERT_DEBUG(fieldAccess != NULL);

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

  if (source == NULL) {
   return NULL;
  }

  result = CreateEmptyVector(context);
  if (result == NULL) {
    return NULL;
  }

  InsertVector(context, result, source);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free access structure with its members and the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeAccessAql (TRI_aql_field_access_t* const fieldAccess) {
  TRI_ASSERT_DEBUG(fieldAccess != NULL);

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

  TRI_ASSERT_DEBUG(source != NULL);
  TRI_ASSERT_DEBUG(source->_fullName != NULL);

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
      fieldAccess->_value._reference._type = source->_value._reference._type;
      fieldAccess->_value._reference._operator = source->_value._reference._operator;
      if (source->_value._reference._type == TRI_AQL_REFERENCE_VARIABLE) {
        fieldAccess->_value._reference._ref._name = source->_value._reference._ref._name;
      }
      else {
        fieldAccess->_value._reference._ref._node = source->_value._reference._ref._node;
      }
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
  if (lhs == NULL) {
    // lhs does not exist, simply return other side
    return 1;
  }

  if (rhs == NULL) {
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

  TRI_ASSERT_DEBUG(context != NULL);
  TRI_ASSERT_DEBUG(candidate != NULL);
  TRI_ASSERT_DEBUG(candidate->_fullName != NULL);

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

  TRI_ASSERT_DEBUG(accesses != NULL);

  found = false;
  n = accesses->_length;
  for (i = 0; i < n; ++i) {
    TRI_aql_field_access_t* existing = (TRI_aql_field_access_t*) TRI_AtVectorPointer(accesses, i);
    int result;

    if (! TRI_EqualString(candidate->_fullName, existing->_fullName)) {
      continue;
    }

    // we found a match
    found = true;

    result = TRI_PickAccessAql(candidate, existing);
    if (result < 0) {
      // candidate is preferred
      TRI_aql_field_access_t* copy;

      // free existing field access
      TRI_ASSERT_DEBUG(existing != NULL);
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
      return TRI_ComparisonOperatorAql(TRI_AQL_NODE_OPERATOR_BINARY_GT);
    case TRI_AQL_RANGE_LOWER_INCLUDED:
      return TRI_ComparisonOperatorAql(TRI_AQL_NODE_OPERATOR_BINARY_GE);
    case TRI_AQL_RANGE_UPPER_EXCLUDED:
      return TRI_ComparisonOperatorAql(TRI_AQL_NODE_OPERATOR_BINARY_LT);
    case TRI_AQL_RANGE_UPPER_INCLUDED:
      return TRI_ComparisonOperatorAql(TRI_AQL_NODE_OPERATOR_BINARY_LE);
    default:
      assert(false);
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the range operator string for a comparison operator
////////////////////////////////////////////////////////////////////////////////

const char* TRI_ComparisonOperatorAql (const TRI_aql_node_type_e type) {
  switch (type) {
    case TRI_AQL_NODE_OPERATOR_BINARY_EQ:
      return "==";
    case TRI_AQL_NODE_OPERATOR_BINARY_NE:
      return "!=";
    case TRI_AQL_NODE_OPERATOR_BINARY_LT:
      return "<";
    case TRI_AQL_NODE_OPERATOR_BINARY_LE:
      return "<=";
    case TRI_AQL_NODE_OPERATOR_BINARY_GT:
      return ">";
    case TRI_AQL_NODE_OPERATOR_BINARY_GE:
      return ">=";
    default:
      assert(false);
  }

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
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

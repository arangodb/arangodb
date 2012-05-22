////////////////////////////////////////////////////////////////////////////////
/// @brief Ahuacatl, internal query functions 
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");

////////////////////////////////////////////////////////////////////////////////
/// @brief type weight used for sorting and comparing
////////////////////////////////////////////////////////////////////////////////

var AHUACATL_TYPEWEIGHT_NULL      = 0;
var AHUACATL_TYPEWEIGHT_BOOL      = 1;
var AHUACATL_TYPEWEIGHT_NUMBER    = 2;
var AHUACATL_TYPEWEIGHT_STRING    = 4;
var AHUACATL_TYPEWEIGHT_LIST      = 8;
var AHUACATL_TYPEWEIGHT_DOCUMENT  = 16;

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief throw a runtime exception
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_THROW (error, data) {
  var err = new ArangoError

  err.errorNum = error.code;
  if (data) {
    err.errorMessage = error.message.replace(/%s/, data);
  }
  else {
    err.errorMessage = error.message;
  }

  throw err;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find an index of a certain type for a collection
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_INDEX (collection, indexTypes) {
  var indexes = collection.getIndexes();

  for (var i = 0; i < indexes.length; ++i) {
    var index = indexes[i];

    for (var j = 0; j < indexTypes.length; ++j) {
      if (index.type == indexTypes[j]) {
        return index.id;
      }
    }
  }

  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone an object
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_CLONE (obj) {
  if (obj == null) {
    return obj;
  }

  if (typeof(obj) != "object") {
    return obj;
  }

  if (Array.isArray(obj)) {
    var copy = [];
    var length = obj.length;
    for (var i = 0; i < length; ++i) {
      copy[i] = AHUACATL_CLONE(obj[i]);
    }
    return copy;
  }

  if (obj instanceof Object) {
    var copy = {};
    for (var attr in obj) {
      if (obj.hasOwnProperty(attr)) {
        copy[attr] = AHUACATL_CLONE(obj[attr]);
      }
    }
    return copy;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate function call argument
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_ARG_CHECK (actualValue, expectedType, functionName) {
  if (AHUACATL_TYPEWEIGHT(actualValue) !== expectedType) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, functionName);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief call a function
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_FCALL (name, parameters) {
  return name.apply(null, parameters);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the numeric value or undefined if it is out of range
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_NUMERIC_VALUE (value) {
  if (isNaN(value) || !isFinite(value)) {
    return null;
  }

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fix a value for a comparison
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_FIX (value) {
  if (value === undefined) {
    return null;
  }

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the name for a type
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_TYPENAME (value) {
  switch (value) {
    case AHUCATL_TYPEWEIGHT_BOOL:
      return 'boolean';
    case AHUCATL_TYPEWEIGHT_NUMBER:
      return 'number';
    case AHUCATL_TYPEWEIGHT_STRING:
      return 'string';
    case AHUCATL_TYPEWEIGHT_LIST:
      return 'list';
    case AHUCATL_TYPEWEIGHT_DOCUMENT:
      return 'document';
    case AHUCATL_TYPEWEIGHT_NULL:
    default:
      return 'null';
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the sort type of an operand
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_TYPEWEIGHT (value) {
  if (value === undefined || value === null) {
    return AHUACATL_TYPEWEIGHT_NULL;
  }

  if (Array.isArray(value)) {
    return AHUACATL_TYPEWEIGHT_LIST;
  }

  switch (typeof(value)) {
    case 'boolean':
      return AHUACATL_TYPEWEIGHT_BOOL;
    case 'number':
      if (isNaN(value) || !isFinite(value)) {
        // not a number => undefined
        return AHUACATL_TYPEWEIGHT_NULL; 
      }
      return AHUACATL_TYPEWEIGHT_NUMBER;
    case 'string':
      return AHUACATL_TYPEWEIGHT_STRING;
    case 'object':
      return AHUACATL_TYPEWEIGHT_DOCUMENT;
  }

  return AHUACATL_TYPEWEIGHT_NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the keys of an array or object in a comparable way
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_KEYS (value, doSort) {
  var keys = [];
  
  if (Array.isArray(value)) {
    var n = value.length;
    for (var j = 0; j < n; ++j) {
      keys.push(j);
    }
  }
  else {
    for (var k in value) {
      if (value.hasOwnProperty(k)) {
        keys.push(k);
      }
    }

    if (doSort) {
      // object keys need to be sorted by names
      keys.sort();
    }
  }

  return keys;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the keys of an array or object in a comparable way
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_KEYLIST (lhs, rhs) {
  var keys = [];
  
  if (Array.isArray(lhs)) {
    // lhs & rhs are lists
    var n;
    if (lhs.length > rhs.length) {
      n = lhs.length;
    }
    else {
      n = rhs.length;
    }
    for (var j = 0; j < n; ++j) {
      keys.push(j);
    }
    return keys;
  }

  // lhs & rhs are arrays
  var k;
  for (k in lhs) {
    keys.push(k);
  }
  for (k in rhs) {
    if (lhs.hasOwnProperty(k)) {
      continue;
    }
    keys.push(k);
  }

  // object keys need to be sorted by names
  keys.sort();

  return keys;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an indexed value from an array or document (e.g. users[3])
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_GET_INDEX (value, index) {
  if (AHUACATL_TYPEWEIGHT(value) == AHUACATL_TYPEWEIGHT_NULL) {
    return null;
  }
  
  if (AHUACATL_TYPEWEIGHT(value) != AHUACATL_TYPEWEIGHT_LIST &&
      AHUACATL_TYPEWEIGHT(value) != AHUACATL_TYPEWEIGHT_DOCUMENT) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_LIST_EXPECTED);
  }

  var result = value[index];

  if (AHUACATL_TYPEWEIGHT(result) === AHUACATL_TYPEWEIGHT_NULL) {
    return null;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an attribute from a document (e.g. users.name)
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_DOCUMENT_MEMBER (value, attributeName) {
  if (AHUACATL_TYPEWEIGHT(value) == AHUACATL_TYPEWEIGHT_NULL) {
    return null;
  }

  if (AHUACATL_TYPEWEIGHT(value) != AHUACATL_TYPEWEIGHT_DOCUMENT) {
    return null;
  }

  var result = value[attributeName];

  if (AHUACATL_TYPEWEIGHT(result) === AHUACATL_TYPEWEIGHT_NULL) {
    return null;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assert that a value is a list, fail otherwise
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_LIST (value) {
  if (AHUACATL_TYPEWEIGHT(value) !== AHUACATL_TYPEWEIGHT_LIST) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_LIST_EXPECTED);
  }

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get all documents from the specified collection
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_GET_DOCUMENTS (collection) {
  return internal.db[collection].all().toArray();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get documents from the specified collection using the primary index
/// (single index value)
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_GET_DOCUMENTS_PRIMARY (collection, idx, id) {
  try {
    return [ internal.db[collection].document(id) ];
  }
  catch (e) {
    return [ ];
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get documents from the specified collection using the primary index
/// (multiple index values)
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_GET_DOCUMENTS_PRIMARY_LIST (collection, idx, values) {
  var result = [ ];

  for (var i in values) {
    var id = values[i];
    try {
      var d = internal.db[collection].document(id);
      result.push(d);
    }
    catch (e) {
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get documents from the specified collection using a hash index
/// (single index value)
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_GET_DOCUMENTS_HASH (collection, idx, example) {
  return internal.db[collection].BY_EXAMPLE_HASH(idx, example).documents;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get documents from the specified collection using a hash index
/// (multiple index values)
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_GET_DOCUMENTS_HASH_LIST (collection, idx, attribute, values) {
  var result = [ ];

  for (var i in values) {
    var value = values[i];
    var example = { };

    example[attribute] = value;

    var documents = internal.db[collection].BY_EXAMPLE_HASH(idx, example).documents;
    for (var j in documents) {
      result.push(documents[j]);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get documents from the specified collection using a skiplist
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_GET_DOCUMENTS_SKIPLIST (collection, idx, example) {
  return internal.db[collection].BY_CONDITION_SKIPLIST(idx, example).documents;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get documents from the specified collection using a skiplist
/// (multiple index values)
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_GET_DOCUMENTS_SKIPLIST_LIST (collection, idx, attribute, values) {
  var result = [ ];

  for (var i in values) {
    var value = values[i];
    var example = { };

    example[attribute] = value;

    var documents = internal.db[collection].BY_EXAMPLE_SKIPLIST(idx, example).documents;
    for (var j in documents) {
      result.push(documents[j]);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get names of all collections
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_COLLECTIONS () {
  var collections = internal.db._collections();
  var result = [ ];

  for (var i = 0; i < collections.length; ++i) {
    result.push({ "_id" : collections[i]._id, "name" : collections[i].name() });
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                logical operations
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief execute ternary operator
///
/// the condition operand must be a boolean value, returns either the truepart
/// or the falsepart 
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_TERNARY_OPERATOR (condition, truePart, falsePart) {
  if (AHUACATL_TYPEWEIGHT(condition) !== AHUACATL_TYPEWEIGHT_BOOL) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_INVALID_LOGICAL_VALUE);
  }

  if (condition) {
    return truePart;
  }
  return falsePart;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform logical and
///
/// both operands must be boolean values, returns a boolean, uses short-circuit
/// evaluation
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_LOGICAL_AND (lhs, rhs) {
  if (AHUACATL_TYPEWEIGHT(lhs) !== AHUACATL_TYPEWEIGHT_BOOL ||
      AHUACATL_TYPEWEIGHT(rhs) !== AHUACATL_TYPEWEIGHT_BOOL) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_INVALID_LOGICAL_VALUE);
  }

  if (!lhs) {
    return false;
  }

  return rhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform logical or
///
/// both operands must be boolean values, returns a boolean, uses short-circuit
/// evaluation
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_LOGICAL_OR (lhs, rhs) {
  if (AHUACATL_TYPEWEIGHT(lhs) !== AHUACATL_TYPEWEIGHT_BOOL ||
      AHUACATL_TYPEWEIGHT(rhs) !== AHUACATL_TYPEWEIGHT_BOOL) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_INVALID_LOGICAL_VALUE);
  }
  
  if (lhs) {
    return true;
  }

  return rhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform logical negation
///
/// the operand must be a boolean values, returns a boolean
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_LOGICAL_NOT (lhs) {
  if (AHUACATL_TYPEWEIGHT(lhs) !== AHUACATL_TYPEWEIGHT_BOOL) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_INVALID_LOGICAL_VALUE);
  }

  return !lhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             comparison operations
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief perform equality check 
///
/// returns true if the operands are equal, false otherwise
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_RELATIONAL_EQUAL (lhs, rhs) {
  var leftWeight = AHUACATL_TYPEWEIGHT(lhs);
  var rightWeight = AHUACATL_TYPEWEIGHT(rhs);

  if (leftWeight != rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= AHUACATL_TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = AHUACATL_KEYLIST(lhs, rhs);

    for (var i in keys) {
      var key = keys[i];
      var result = AHUACATL_RELATIONAL_EQUAL(lhs[key], rhs[key]);
      if (result === false) {
        return result;
      }
    }
    return true;
  }

  // primitive type
  if (AHUACATL_TYPEWEIGHT(lhs) === AHUACATL_TYPEWEIGHT_NULL) {
    lhs = null;
  }
  if (AHUACATL_TYPEWEIGHT(rhs) === AHUACATL_TYPEWEIGHT_NULL) {
    rhs = null;
  }

  return (lhs === rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform inequality check 
///
/// returns true if the operands are unequal, false otherwise
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_RELATIONAL_UNEQUAL (lhs, rhs) {
  var leftWeight = AHUACATL_TYPEWEIGHT(lhs);
  var rightWeight = AHUACATL_TYPEWEIGHT(rhs);
  
  if (leftWeight != rightWeight) {
    return true;
  }

  // lhs and rhs have the same type

  if (leftWeight >= AHUACATL_TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = AHUACATL_KEYLIST(lhs, rhs);

    for (var i in keys) {
      var key = keys[i];
      var result = AHUACATL_RELATIONAL_UNEQUAL(lhs[key], rhs[key]);
      if (result === true) {
        return result;
      }
    }

    return false;
  }

  // primitive type
  if (AHUACATL_TYPEWEIGHT(lhs) === AHUACATL_TYPEWEIGHT_NULL) {
    lhs = null;
  }
  if (AHUACATL_TYPEWEIGHT(rhs) === AHUACATL_TYPEWEIGHT_NULL) {
    rhs = null;
  }

  return (lhs !== rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform greater than check (inner function)
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_RELATIONAL_GREATER_REC (lhs, rhs) {
  var leftWeight = AHUACATL_TYPEWEIGHT(lhs);
  var rightWeight = AHUACATL_TYPEWEIGHT(rhs);
  
  if (leftWeight > rightWeight) {
    return true;
  }
  if (leftWeight < rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= AHUACATL_TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = AHUACATL_KEYLIST(lhs, rhs);

    for (var i in keys) {
      var key = keys[i];
      var result = AHUACATL_RELATIONAL_GREATER_REC(lhs[key], rhs[key]);
      if (result !== null) {
        return result;
      }
    }
    
    return null;
  }

  // primitive type
  if (AHUACATL_TYPEWEIGHT(lhs) === AHUACATL_TYPEWEIGHT_NULL) {
    lhs = null;
  }
  if (AHUACATL_TYPEWEIGHT(rhs) === AHUACATL_TYPEWEIGHT_NULL) {
    rhs = null;
  }

  if (lhs === rhs) {
    return null;
  }

  return (lhs > rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform greater than check 
///
/// returns true if the left operand is greater than the right operand
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_RELATIONAL_GREATER (lhs, rhs) {
  var result = AHUACATL_RELATIONAL_GREATER_REC(lhs, rhs);

  if (result === null) {
    result = false;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform greater equal check (inner function)
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_RELATIONAL_GREATEREQUAL_REC (lhs, rhs) {
  var leftWeight = AHUACATL_TYPEWEIGHT(lhs);
  var rightWeight = AHUACATL_TYPEWEIGHT(rhs);
  
  if (leftWeight > rightWeight) {
    return true;
  }
  if (leftWeight < rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= AHUACATL_TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = AHUACATL_KEYLIST(lhs, rhs);

    for (var i in keys) {
      var key = keys[i];
      var result = AHUACATL_RELATIONAL_GREATEREQUAL_REC(lhs[key], rhs[key]);
      if (result !== null) {
        return result;
      }
    }
    
    return null;
  }

  // primitive type
  if (AHUACATL_TYPEWEIGHT(lhs) === AHUACATL_TYPEWEIGHT_NULL) {
    lhs = null;
  }
  if (AHUACATL_TYPEWEIGHT(rhs) === AHUACATL_TYPEWEIGHT_NULL) {
    rhs = null;
  }

  if (lhs === rhs) {
    return null;
  }

  return (lhs >= rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform greater equal check 
///
/// returns true if the left operand is greater or equal to the right operand
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_RELATIONAL_GREATEREQUAL (lhs, rhs) {
  var result = AHUACATL_RELATIONAL_GREATEREQUAL_REC(lhs, rhs);

  if (result === null) {
    result = true;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform less than check (inner function)
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_RELATIONAL_LESS_REC (lhs, rhs) {
  var leftWeight = AHUACATL_TYPEWEIGHT(lhs);
  var rightWeight = AHUACATL_TYPEWEIGHT(rhs);
  
  if (leftWeight < rightWeight) {
    return true;
  }
  if (leftWeight > rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= AHUACATL_TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = AHUACATL_KEYLIST(lhs, rhs);

    for (var i in keys) {
      var key = keys[i];
      var result = AHUACATL_RELATIONAL_LESS_REC(lhs[key], rhs[key]);
      if (result !== null) {
        return result;
      }
    }
    
    return null;
  }

  // primitive type
  if (AHUACATL_TYPEWEIGHT(lhs) === AHUACATL_TYPEWEIGHT_NULL) {
    lhs = null;
  }
  if (AHUACATL_TYPEWEIGHT(rhs) === AHUACATL_TYPEWEIGHT_NULL) {
    rhs = null;
  }

  if (lhs === rhs) {
    return null;
  }

  return (lhs < rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform less than check 
///
/// returns true if the left operand is less than the right operand
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_RELATIONAL_LESS (lhs, rhs) {
  var result = AHUACATL_RELATIONAL_LESS_REC(lhs, rhs);

  if (result === null) {
    result = false;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform less equal check (inner function)
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_RELATIONAL_LESSEQUAL_REC (lhs, rhs) {
  var leftWeight = AHUACATL_TYPEWEIGHT(lhs);
  var rightWeight = AHUACATL_TYPEWEIGHT(rhs);
  
  if (leftWeight < rightWeight) {
    return true;
  }
  if (leftWeight > rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= AHUACATL_TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = AHUACATL_KEYLIST(lhs, rhs);

    for (var i in keys) {
      var key = keys[i];
      var result = AHUACATL_RELATIONAL_LESSEQUAL_REC(lhs[key], rhs[key]);
      if (result !== null) {
        return result;
      }
    }

    return null;
  }
  
  // primitive type
  if (AHUACATL_TYPEWEIGHT(lhs) === AHUACATL_TYPEWEIGHT_NULL) {
    lhs = null;
  }
  if (AHUACATL_TYPEWEIGHT(rhs) === AHUACATL_TYPEWEIGHT_NULL) {
    rhs = null;
  }
  
  if (lhs === rhs) {
    return null;
  }

  return (lhs <= rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform less equal check 
///
/// returns true if the left operand is less or equal to the right operand
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_RELATIONAL_LESSEQUAL (lhs, rhs) {
  var result = AHUACATL_RELATIONAL_LESSEQUAL_REC(lhs, rhs);

  if (result === null) {
    result = true;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform comparison
///
/// returns -1 if the left operand is less than the right operand, 1 if it is
/// greater, 0 if both operands are equal
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_RELATIONAL_CMP (lhs, rhs) {
  var leftWeight = AHUACATL_TYPEWEIGHT(lhs);
  var rightWeight = AHUACATL_TYPEWEIGHT(rhs);
  
  if (leftWeight < rightWeight) {
    return -1;
  }
  if (leftWeight > rightWeight) {
    return 1;
  }

  // lhs and rhs have the same type

  if (leftWeight >= AHUACATL_TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = AHUACATL_KEYLIST(lhs, rhs);

    for (var i in keys) {
      var key = keys[i];
      var result = AHUACATL_RELATIONAL_CMP(lhs[key], rhs[key]);
      if (result !== 0) {
        return result;
      }
    }
    
    return 0;
  }

  // primitive type
  if (AHUACATL_TYPEWEIGHT(lhs) === AHUACATL_TYPEWEIGHT_NULL) {
    lhs = null;
  }
  if (AHUACATL_TYPEWEIGHT(rhs) === AHUACATL_TYPEWEIGHT_NULL) {
    rhs = null;
  }

  if (lhs < rhs) {
    return -1;
  }
   
  if (lhs > rhs) {
    return 1;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform in list check 
///
/// returns true if the left operand is contained in the right operand
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_RELATIONAL_IN (lhs, rhs) {
  var leftWeight = AHUACATL_TYPEWEIGHT(lhs);
  var rightWeight = AHUACATL_TYPEWEIGHT(rhs);
  
  if (rightWeight !== AHUACATL_TYPEWEIGHT_LIST) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_LIST_EXPECTED);
  }
  
  var r = AHUACATL_KEYS(rhs, false);
  var numRight = r.length;

  for (var i = 0; i < numRight; ++i) {
    var key = r[i];
    if (AHUACATL_RELATIONAL_EQUAL(lhs, rhs[key])) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             arithmetic operations
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief perform unary plus operation
///
/// the operand must be numeric or this function will fail
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_UNARY_PLUS (value) {
  if (AHUACATL_TYPEWEIGHT(value) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
  }

  var result = AHUACATL_NUMERIC_VALUE(value);
  if (AHUACATL_TYPEWEIGHT(result) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform unary minus operation
///
/// the operand must be numeric or this function will fail
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_UNARY_MINUS (value) {
  if (AHUACATL_TYPEWEIGHT(value) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
  }

  var result = AHUACATL_NUMERIC_VALUE(-value);
  if (AHUACATL_TYPEWEIGHT(result) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic plus
///
/// both operands must be numeric or this function will fail
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_ARITHMETIC_PLUS (lhs, rhs) { 
  if (AHUACATL_TYPEWEIGHT(lhs) !== AHUACATL_TYPEWEIGHT_NUMBER ||
      AHUACATL_TYPEWEIGHT(rhs) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
  }

  var result = AHUACATL_NUMERIC_VALUE(lhs + rhs);
  if (AHUACATL_TYPEWEIGHT(result) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic minus
///
/// both operands must be numeric or this function will fail
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_ARITHMETIC_MINUS (lhs, rhs) {
  if (AHUACATL_TYPEWEIGHT(lhs) !== AHUACATL_TYPEWEIGHT_NUMBER ||
      AHUACATL_TYPEWEIGHT(rhs) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
  }

  var result = AHUACATL_NUMERIC_VALUE(lhs - rhs);
  if (AHUACATL_TYPEWEIGHT(result) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic multiplication
///
/// both operands must be numeric or this function will fail
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_ARITHMETIC_TIMES (lhs, rhs) {
  if (AHUACATL_TYPEWEIGHT(lhs) !== AHUACATL_TYPEWEIGHT_NUMBER ||
      AHUACATL_TYPEWEIGHT(rhs) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
  }

  var result = AHUACATL_NUMERIC_VALUE(lhs * rhs);
  if (AHUACATL_TYPEWEIGHT(result) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic division
///
/// both operands must be numeric or this function will fail
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_ARITHMETIC_DIVIDE (lhs, rhs) {
  if (AHUACATL_TYPEWEIGHT(lhs) !== AHUACATL_TYPEWEIGHT_NUMBER ||
      AHUACATL_TYPEWEIGHT(rhs) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
  }
  
  if (rhs == 0) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_DIVISION_BY_ZERO);
  }

  var result = AHUACATL_NUMERIC_VALUE(lhs / rhs);
  if (AHUACATL_TYPEWEIGHT(result) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic modulus
///
/// both operands must be numeric or this function will fail
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_ARITHMETIC_MODULUS (lhs, rhs) {
  if (AHUACATL_TYPEWEIGHT(lhs) !== AHUACATL_TYPEWEIGHT_NUMBER ||
      AHUACATL_TYPEWEIGHT(rhs) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
  }

  if (rhs == 0) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_DIVISION_BY_ZERO);
  }

  var result  = AHUACATL_NUMERIC_VALUE(lhs % rhs);
  if (AHUACATL_TYPEWEIGHT(result) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  string functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief perform string concatenation
///
/// all operands must be strings or this function will fail
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_STRING_CONCAT () {
  var result = '';

  for (var i in arguments) {
    var element = arguments[i];

    if (AHUACATL_TYPEWEIGHT(element) === AHUACATL_TYPEWEIGHT_NULL) {
      continue;
    }

    AHUACATL_ARG_CHECK(element, AHUACATL_TYPEWEIGHT_STRING, "CONCAT");

    result += element;
  }

  return result; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform string concatenation using a separator character
///
/// all operands must be strings or this function will fail
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_STRING_CONCAT_SEPARATOR () {
  var separator;
  var found = false;
  var result = '';

  for (var i in arguments) {
    var element = arguments[i];

    if (i > 0 && AHUACATL_TYPEWEIGHT(element) === AHUACATL_TYPEWEIGHT_NULL) {
      continue;
    }
    
    AHUACATL_ARG_CHECK(element, AHUACATL_TYPEWEIGHT_STRING, "CONCAT_SEPARATOR");

    if (i == 0) {
      separator = element;
      continue;
    }
    else if (found) {
      result += separator;
    }

    found = true;

    result += element;
  }

  return result; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the length of a string in characters (not bytes)
///
/// the input operand must be a string or this function will fail
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_STRING_LENGTH (value) {
  AHUACATL_ARG_CHECK(value, AHUACATL_TYPEWEIGHT_STRING, "STRING_LENGTH");

  return value.length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to lower case
///
/// the input operand must be a string or this function will fail
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_STRING_LOWER (value) {
  AHUACATL_ARG_CHECK(value, AHUACATL_TYPEWEIGHT_STRING, "LOWER");

  return value.toLowerCase();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to upper case
///
/// the input operand must be a string or this function will fail
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_STRING_UPPER (value) {
  AHUACATL_ARG_CHECK(value, AHUACATL_TYPEWEIGHT_STRING, "UPPER");

  return value.toUpperCase();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a substring of the string
///
/// the input operand must be a string or this function will fail
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_STRING_SUBSTRING (value, offset, count) {
  AHUACATL_ARG_CHECK(value, AHUACATL_TYPEWEIGHT_STRING, "SUBSTRING");
  AHUACATL_ARG_CHECK(offset, AHUACATL_TYPEWEIGHT_NUMBER, "SUBSTRING");

  return value.substr(offset, count);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                typecast functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to null
///
/// the operand can have any type, always returns null
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_CAST_NULL (value) {
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to a bool
///
/// the operand can have any type, always returns a bool
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_CAST_BOOL (value) {
  switch (AHUACATL_TYPEWEIGHT(value)) {
    case AHUACATL_TYPEWEIGHT_NULL:
      return false;
    case AHUACATL_TYPEWEIGHT_BOOL:
      return value;
    case AHUACATL_TYPEWEIGHT_NUMBER:
      return (value != 0);
    case AHUACATL_TYPEWEIGHT_STRING: 
      return (value != '');
    case AHUACATL_TYPEWEIGHT_LIST:
      return (value.length > 0);
    case AHUACATL_TYPEWEIGHT_DOCUMENT:
      return (AHUACATL_KEYS(value, false).length > 0);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to a number
///
/// the operand can have any type, always returns a number
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_CAST_NUMBER (value) {
  switch (AHUACATL_TYPEWEIGHT(value)) {
    case AHUACATL_TYPEWEIGHT_NULL:
    case AHUACATL_TYPEWEIGHT_LIST:
    case AHUACATL_TYPEWEIGHT_DOCUMENT:
      return 0.0;
    case AHUACATL_TYPEWEIGHT_BOOL:
      return (value ? 1 : 0);
    case AHUACATL_TYPEWEIGHT_NUMBER:
      return value;
    case AHUACATL_TYPEWEIGHT_STRING:
      var result = parseFloat(value);
      return ((AHUACATL_TYPEWEIGHT(result) === AHUACATL_TYPEWEIGHT_NUMBER) ? result : 0);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to a string
///
/// the operand can have any type, always returns a string
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_CAST_STRING (value) {
  switch (AHUACATL_TYPEWEIGHT(value)) {
    case AHUACATL_TYPEWEIGHT_STRING:
      return value;
    case AHUACATL_TYPEWEIGHT_NULL:
      return 'null';
    case AHUACATL_TYPEWEIGHT_BOOL:
      return (value ? 'true' : 'false');
    case AHUACATL_TYPEWEIGHT_NUMBER:
    case AHUACATL_TYPEWEIGHT_LIST:
    case AHUACATL_TYPEWEIGHT_DOCUMENT:
      return value.toString();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               typecheck functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type null
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_IS_NULL (value) {
  return (AHUACATL_TYPEWEIGHT(value) === AHUACATL_TYPEWEIGHT_NULL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type bool
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_IS_BOOL (value) {
  return (AHUACATL_TYPEWEIGHT(value) === AHUACATL_TYPEWEIGHT_BOOL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type number
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_IS_NUMBER (value) {
  return (AHUACATL_TYPEWEIGHT(value) === AHUACATL_TYPEWEIGHT_NUMBER);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type string
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_IS_STRING (value) {
  return (AHUACATL_TYPEWEIGHT(value) === AHUACATL_TYPEWEIGHT_STRING);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type list
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_IS_LIST (value) {
  return (AHUACATL_TYPEWEIGHT(value) === AHUACATL_TYPEWEIGHT_LIST);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type document
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_IS_DOCUMENT (value) {
  return (AHUACATL_TYPEWEIGHT(value) === AHUACATL_TYPEWEIGHT_DOCUMENT);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 numeric functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief integer closest to value, not greater than value
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_NUMBER_FLOOR (value) {
  if (!AHUACATL_IS_NUMBER(value)) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FLOOR");
  }
  
  return Math.floor(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief integer closest to value and not less than value
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_NUMBER_CEIL (value) {
  if (!AHUACATL_IS_NUMBER(value)) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "CEIL");
  }
  
  return Math.ceil(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief integer closest to value 
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_NUMBER_ROUND (value) {
  if (!AHUACATL_IS_NUMBER(value)) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "ROUND");
  }
  
  return Math.round(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief absolute value
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_NUMBER_ABS (value) {
  if (!AHUACATL_IS_NUMBER(value)) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "ABS");
  }
  
  return Math.abs(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a random value between 0 and 1
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_NUMBER_RAND () {
  return Math.random();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        high level query functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief sort the results
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_SORT (value, sortFunction) {
  AHUACATL_LIST(value);
 
  var n = value.length;
  if (n > 0) {
    value.sort(sortFunction);
  }

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief group the results
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_GROUP (value, sortFunction, groupFunction, into) {
  AHUACATL_LIST(value);

  var n = value.length;
  if (n == 0) {
    return [ ];
  }

  AHUACATL_SORT(value, sortFunction);

  var result = [ ];
  var currentGroup = undefined;
  var oldGroup = undefined;
  
  for (var i = 0; i < n; ++i) {
    var row = value[i];
    var groupValue = groupFunction(row);

    if (AHUACATL_RELATIONAL_UNEQUAL(oldGroup, groupValue)) {
      oldGroup = AHUACATL_CLONE(groupValue);

      if (currentGroup) {
        result.push(AHUACATL_CLONE(currentGroup));
      }
      
      currentGroup = groupValue;
      if (into) {
        currentGroup[into] = [ ];
      }
    }

    if (into) {
      currentGroup[into].push(AHUACATL_CLONE(row));
    }
  }

  if (currentGroup) {
    result.push(AHUACATL_CLONE(currentGroup));
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief limit the number of results
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_LIMIT (value, offset, count) {
  AHUACATL_LIST(value);

  if (count < 0) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE);
  }

  return value.slice(offset, offset + count);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                         list processing functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get the length of a list
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_LENGTH () {
  var value = arguments[0];

  AHUACATL_LIST(value);

  return value.length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the union (all) of all arguments
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_UNION () {
  var result = [ ];

  for (var i in arguments) {
    var element = arguments[i];

    if (AHUACATL_TYPEWEIGHT(element) !== AHUACATL_TYPEWEIGHT_LIST) {
      AHUACATL_THROW(internal.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "UNION");
    }

    for (var k in element) {
      if (!element.hasOwnProperty(k)) {
        continue;
      }

      result.push(element[k]);
    }
  }

  return result; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum of all values
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_MAX () {
  var result = null;
  var value = arguments[0];

  AHUACATL_LIST(value);

  for (var i in value) {
    var currentValue = value[i];
    
    if (AHUACATL_TYPEWEIGHT(currentValue) === AHUACATL_TYPEWEIGHT_NULL) {
      continue;
    }

    if (result === null || AHUACATL_RELATIONAL_GREATER(currentValue, result)) {
      result = currentValue;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief minimum of all values
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_MIN () {
  var result = null;
  var value = arguments[0];

  AHUACATL_LIST(value);

  for (var i in value) {
    var currentValue = value[i];
    
    if (AHUACATL_TYPEWEIGHT(currentValue) === AHUACATL_TYPEWEIGHT_NULL) {
      continue;
    }
    
    if (result === null || AHUACATL_RELATIONAL_LESS(currentValue, result)) {
      result = currentValue;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sum of all values
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_SUM () {
  var result = null;
  var value = arguments[0];

  AHUACATL_LIST(value);

  for (var i in value) {
    var currentValue = value[i];
    
    if (AHUACATL_TYPEWEIGHT(currentValue) === AHUACATL_TYPEWEIGHT_NULL) {
      continue;
    }

    if (AHUACATL_TYPEWEIGHT(currentValue) !== AHUACATL_TYPEWEIGHT_NUMBER) {
      AHUACATL_THROW(internal.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "SUM");
    }
    
    if (result === null) {
      result = currentValue;
    }
    else {
      result += currentValue;
    }
  }

  return AHUACATL_NUMERIC_VALUE(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     geo functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return at most <limit> documents near a certain point
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_GEO_NEAR () {
  var collection = arguments[0];
  var latitude = arguments[1];
  var longitude = arguments[2];
  var limit = arguments[3];
  var distanceAttribute = arguments[4];

  var idx = AHUACATL_INDEX(internal.db[collection], [ "geo1", "geo2" ]); 
  if (idx == null) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_GEO_INDEX_MISSING, collection);
  }

  var result = internal.db[collection].NEAR(idx, latitude, longitude, limit);
  if (distanceAttribute == null) {
    return result.documents;
  }

  // inject distances
  var documents = result.documents;
  var distances = result.distances;
  var n = documents.length;
  for (var i = 0; i < n; ++i) {
    documents[i][distanceAttribute] = distances[i];
  }

  return documents;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return documents within <radius> around a certain point
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_GEO_WITHIN () {
  var collection = arguments[0];
  var latitude = arguments[1];
  var longitude = arguments[2];
  var radius = arguments[3];
  var distanceAttribute = arguments[4];

  var idx = AHUACATL_INDEX(internal.db[collection], [ "geo1", "geo2" ]); 
  if (idx == null) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_GEO_INDEX_MISSING, collection);
  }

  var result = internal.db[collection].WITHIN(idx, latitude, longitude, radius);
  if (distanceAttribute == null) {
    return result.documents;
  }

  // inject distances
  var documents = result.documents;
  var distances = result.distances;
  var n = documents.length;
  for (var i = 0; i < n; ++i) {
    documents[i][distanceAttribute] = distances[i];
  }

  return documents;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   graph functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief find all paths through a graph
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_GRAPH_PATHS () {
  var collection = arguments[0];
  var edgeType = arguments[1];
  var minLength = arguments[2] != undefined ? arguments[2] : 1;
  var maxLength = arguments[3] != undefined ? arguments[3] : 10;
  var followCycles = arguments[4] ? arguments[4] : false;
  var direction;

  // validate arguments
  if (edgeType == "outbound") {
    direction = 1;
  }
  else if (edgeType == "inbound") {
    direction = 2;
  }
  else if (edgeType == "any") {
    direction = 3;
  }
  else {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "PATHS");
  }

  if (minLength < 0 || maxLength < 0 || minLength > maxLength) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "PATHS");
  }

  var edgeCollection = internal.edges[collection];
  var searchAttributes = { 
    "edgeCollection" : edgeCollection, 
    "minLength" : minLength, 
    "maxLength" : maxLength, 
    "direction" : direction,
    "followCycles" : followCycles,
  };

  var allEdges = edgeCollection.all().toArray();
  // TODO: restrict allEdges to edges with certain _from values etc.

  var result = [ ];
  var n = allEdges.length;
  for (var i = 0; i < n; ++i) {

    var edge = allEdges[i];
    var _from = edge._from; 
    var _to = edge._to; 
    var sources = [ ];

    if (searchAttributes.direction & 1) {
      sources.push(_from);
    }
    if (searchAttributes.direction & 2) {
      sources.push(_to);
    }

    for (var j = 0; j < sources.length; ++j) {
      var visited = { };
      var vertices = [ ];
      var edges = [ ];

      var source = sources[j];
      var next = internal.db._document(source);
      vertices.push(next);
      if (!searchAttributes.followCycles) {
        visited[source] = true;
      }
    
      if (searchAttributes.minLength == 0) {
        var copy = AHUACATL_CLONE(vertices);
        result.push({ "vertices" : copy, "edges" : [ ], "source" : copy[0], "destination" : copy[copy.length - 1] });
      }

      if (searchAttributes.maxLength > 0) {
        var subResult = AHUACATL_GRAPH_SUBNODES(searchAttributes, AHUACATL_CLONE(visited), edges, vertices, edge, 0);
        for (var k = 0; k < subResult.length; ++k) {
          result.push(subResult[k]);
        }
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find all paths through a graph
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_GRAPH_SUBNODES (searchAttributes, visited, edges, vertices, edge, level) {
  var result = [ ];

  var _from = edge._from; 
  var _to = edge._to;
  var targets = [ ];

  if (searchAttributes.direction & 1) {
    targets.push(_to);
  }
  if (searchAttributes.direction & 2) {
    targets.push(_from);
  }

  for (var i = 0; i < targets.length; ++i) {
    var target = targets[i];

    if (!searchAttributes.followCycles && visited[target]) {
      continue;
    }

    var clonedEdges = AHUACATL_CLONE(edges);
    var clonedVertices = AHUACATL_CLONE(vertices);

    clonedEdges.push(edge);
    var vertex = internal.db._document(target);
    clonedVertices.push(vertex);
      
    if (level + 1 >= searchAttributes.minLength) {
      result.push({ "vertices" : clonedVertices, "edges" : clonedEdges, "source" : clonedVertices[0], "destination" : clonedVertices[clonedVertices.length - 1] });
    }

    if (level + 1 < searchAttributes.maxLength) {
      // recursion

      if (!searchAttributes.followCycles) {
        visited[target] = true;
      }

      var subEdges;
      if (searchAttributes.direction == 1) {
        subEdges = searchAttributes.edgeCollection.outEdges(vertex);
      }
      else if (searchAttributes.direction == 2) {
        subEdges = searchAttributes.edgeCollection.inEdges(vertex);
      }
      else if (searchAttributes.direction == 3) {
        subEdges = searchAttributes.edgeCollection.edges(vertex);
      }

      for (var j = 0; j < subEdges.length; ++j) {
        var subResult = AHUACATL_GRAPH_SUBNODES(searchAttributes, AHUACATL_CLONE(visited), clonedEdges, clonedVertices, subEdges[j], level + 1);

        for (var k = 0; k < subResult.length; ++k) {
          result.push(subResult[k]);
        }
      }

      if (!searchAttributes.followCycles) {
        delete visited[target];
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    misc functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Ahuacatl
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief merge all arguments
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_MERGE () {
  var result = { };

  for (var i in arguments) {
    var element = arguments[i];

    if (AHUACATL_TYPEWEIGHT(element) !== AHUACATL_TYPEWEIGHT_DOCUMENT) {
      AHUACATL_THROW(internal.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "MERGE");
    }

    for (var k in element) {
      if (!element.hasOwnProperty(k)) {
        continue;
      }

      result[k] = element[k];
    }
  }

  return result; 
}


////////////////////////////////////////////////////////////////////////////////
/// @brief passthru the argument
///
/// this function is marked as non-deterministic so its argument withstands
/// query optimisation. this function can be used for testing
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_PASSTHRU () {
  var value = arguments[0];

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief always fail
///
/// this function is non-deterministic so it is not executed at query 
/// optimisation time. this function can be used for testing
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_FAIL () {
  var message = arguments[0];

  if (AHUACATL_TYPEWEIGHT(message) === AHUACATL_TYPEWEIGHT_STRING) {
    AHUACATL_THROW(internal.errors.ERROR_QUERY_FAIL_CALLED, message);
  }

  AHUACATL_THROW(internal.errors.ERROR_QUERY_FAIL_CALLED, "");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


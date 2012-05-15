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

function AHUACATL_ARG_CHECK (actualValue, expectedType, functionName, argument) {
  if (AHUACATL_TYPEWEIGHT(actualValue) !== expectedType) {
    throw "expecting " + AHUACATL_TYPENAME(expectedType) + " for argument " + argument + " of function " + functionName;
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
    throw "expecting list or document for index access";
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
    throw "expecting list";
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
/// @brief get documents from the specified collection using a hash index
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_GET_DOCUMENTS_HASH (collection, idx, example) {
  return internal.db[collection].BY_EXAMPLE_HASH(idx, example).documents;
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
/// @brief perform logical and
///
/// both operands must be boolean values, returns a boolean, uses short-circuit
/// evaluation
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_LOGICAL_AND (lhs, rhs) {
  if (AHUACATL_TYPEWEIGHT(lhs) !== AHUACATL_TYPEWEIGHT_BOOL ||
      AHUACATL_TYPEWEIGHT(rhs) !== AHUACATL_TYPEWEIGHT_BOOL) {
    throw "expecting bool operands for and";
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
    throw "expecting bool operands for or";
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
    throw "expecting bool operand for not";
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
    throw "expecting list for in";
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
    throw "expecting number for unary plus";
  }

  var result = AHUACATL_NUMERIC_VALUE(value);
  if (AHUACATL_TYPEWEIGHT(result) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    throw "number out of range";
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
    throw "expecting number for unary minus";
  }

  var result = AHUACATL_NUMERIC_VALUE(-value);
  if (AHUACATL_TYPEWEIGHT(result) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    throw "number out of range";
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
    throw "expecting numbers for plus";
  }

  var result = AHUACATL_NUMERIC_VALUE(lhs + rhs);
  if (AHUACATL_TYPEWEIGHT(result) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    throw "number out of range";
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
    throw "expecting numbers for minus";
  }

  var result = AHUACATL_NUMERIC_VALUE(lhs - rhs);
  if (AHUACATL_TYPEWEIGHT(result) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    throw "number out of range";
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
    throw "expecting numbers for times";
  }

  var result = AHUACATL_NUMERIC_VALUE(lhs * rhs);
  if (AHUACATL_TYPEWEIGHT(result) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    throw "number out of range";
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
    throw "expecting numbers for div";
  }
  
  if (rhs == 0) {
    throw "division by zero";
  }

  var result = AHUACATL_NUMERIC_VALUE(lhs / rhs);
  if (AHUACATL_TYPEWEIGHT(result) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    throw "number out of range";
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
    throw "expecting numbers for mod";
  }

  if (rhs == 0) {
    throw "division by zero";
  }

  var result  = AHUACATL_NUMERIC_VALUE(lhs % rhs);
  if (AHUACATL_TYPEWEIGHT(result) !== AHUACATL_TYPEWEIGHT_NUMBER) {
    throw "number out of range";
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

    AHUACATL_ARG_CHECK(element, AHUACATL_TYPEWEIGHT_STRING, "CONCAT", i + 1);

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
    
    AHUACATL_ARG_CHECK(element, AHUACATL_TYPEWEIGHT_STRING, "CONCAT_SEPARATOR", i + 1);

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
  AHUACATL_ARG_CHECK(value, AHUACATL_TYPEWEIGHT_STRING, "STRING_LENGTH", 1);

  return value.length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to lower case
///
/// the input operand must be a string or this function will fail
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_STRING_LOWER (value) {
  AHUACATL_ARG_CHECK(value, AHUACATL_TYPEWEIGHT_STRING, "LOWER", 1);

  return value.toLowerCase();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to upper case
///
/// the input operand must be a string or this function will fail
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_STRING_UPPER (value) {
  AHUACATL_ARG_CHECK(value, AHUACATL_TYPEWEIGHT_STRING, "UPPER", 1);

  return value.toUpperCase();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a substring of the string
///
/// the input operand must be a string or this function will fail
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_STRING_SUBSTRING (value, offset, count) {
  AHUACATL_ARG_CHECK(value, AHUACATL_TYPEWEIGHT_STRING, "SUBSTRING", 1);
  AHUACATL_ARG_CHECK(offset, AHUACATL_TYPEWEIGHT_NUMBER, "SUBSTRING", 2);

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
    throw "expecting number for floor";
  }
  
  return Math.floor(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief integer closest to value and not less than value
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_NUMBER_CEIL (value) {
  if (!AHUACATL_IS_NUMBER(value)) {
    throw "expecting number for ceil";
  }
  
  return Math.ceil(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief integer closest to value 
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_NUMBER_ROUND (value) {
  if (!AHUACATL_IS_NUMBER(value)) {
    throw "expecting number for round";
  }
  
  return Math.round(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief absolute value
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_NUMBER_ABS (value) {
  if (!AHUACATL_IS_NUMBER(value)) {
    throw "expecting number for abs";
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
    throw "negative count is not supported for limit";
  }

  return value.slice(offset, offset + count);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the length of a list
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_LENGTH () {
  var value = arguments[0];

  AHUACATL_LIST(value);

  return value.length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge all arguments
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_MERGE () {
  var result = { };

  for (var i in arguments) {
    var element = arguments[i];

    if (AHUACATL_TYPEWEIGHT(element) !== AHUACATL_TYPEWEIGHT_DOCUMENT) {
      throw "expecting documents for merge";
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
/// @brief create the union (all) of all arguments
////////////////////////////////////////////////////////////////////////////////

function AHUACATL_UNION () {
  var result = [ ];

  for (var i in arguments) {
    var element = arguments[i];

    if (AHUACATL_TYPEWEIGHT(element) !== AHUACATL_TYPEWEIGHT_LIST) {
      throw "expecting lists for union";
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
      throw "expecting number for sum";
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


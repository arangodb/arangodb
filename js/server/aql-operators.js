////////////////////////////////////////////////////////////////////////////////
/// @brief AQL query language operators (internal use only)
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

var AQL_TYPEWEIGHT_UNDEFINED = 0;
var AQL_TYPEWEIGHT_NULL      = 1;
var AQL_TYPEWEIGHT_BOOL      = 2;
var AQL_TYPEWEIGHT_NUMBER    = 3;
var AQL_TYPEWEIGHT_STRING    = 4;
var AQL_TYPEWEIGHT_ARRAY     = 5;
var AQL_TYPEWEIGHT_OBJECT    = 6;

////////////////////////////////////////////////////////////////////////////////
/// @brief get the sort type of an operand
////////////////////////////////////////////////////////////////////////////////

function AQL_TYPEWEIGHT (lhs) {
  if (lhs === undefined) {
    return AQL_TYPEWEIGHT_UNDEFINED;
  }

  if (lhs === null) {
    return AQL_TYPEWEIGHT_NULL;
  }

  if (lhs instanceof Array) {
    return AQL_TYPEWEIGHT_ARRAY;
  }

  switch (typeof(lhs)) {
    case 'boolean':
      return AQL_TYPEWEIGHT_BOOL;
    case 'number':
      if (isNaN(lhs) || !isFinite(lhs)) {
        // not a number => undefined
        return AQL_TYPEWEIGHT_UNDEFINED; 
      }
      return AQL_TYPEWEIGHT_NUMBER;
    case 'string':
      return AQL_TYPEWEIGHT_STRING;
    case 'object':
      return AQL_TYPEWEIGHT_OBJECT;
    default:
      return AQL_TYPEWEIGHT_UNDEFINED;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the keys of an array or object in a comparable way
////////////////////////////////////////////////////////////////////////////////

function AQL_KEYS (lhs) {
  var keys = [];
  
  if (lhs instanceof Array) {
    var i = 0;
    for (var k in lhs) {
      if (lhs.hasOwnProperty(k)) {
        keys.push(i++);
      }
    }
  }
  else {
    for (var k in lhs) {
      if (lhs.hasOwnProperty(k)) {
        keys.push(k);
      }
    }

    // object keys need to be sorted by names
    keys.sort();
  }

  return keys;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform logical and
////////////////////////////////////////////////////////////////////////////////

function AQL_LOGICAL_AND (lhs, rhs) {
  // lhs && rhs: both operands must be bools, returns a bool or undefined
  if (AQL_TYPEWEIGHT(lhs) !== AQL_TYPEWEIGHT_BOOL ||
      AQL_TYPEWEIGHT(rhs) !== AQL_TYPEWEIGHT_BOOL) {
    return undefined; 
  }

  return (lhs && rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform logical or
////////////////////////////////////////////////////////////////////////////////

function AQL_LOGICAL_OR (lhs, rhs) {
  // lhs || rhs: both operands must be bools, returns a bool or undefined
  if (AQL_TYPEWEIGHT(lhs) !== AQL_TYPEWEIGHT_BOOL ||
      AQL_TYPEWEIGHT(rhs) !== AQL_TYPEWEIGHT_BOOL) {
    return undefined; 
  }

  return (lhs || rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform logical negation
////////////////////////////////////////////////////////////////////////////////

function AQL_LOGICAL_NOT (lhs) {
  // !lhs: operand must be bool, returns a bool or undefined
  if (AQL_TYPEWEIGHT(lhs) !== AQL_TYPEWEIGHT_BOOL) {
    return undefined;
  }

  return !lhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform equality check 
////////////////////////////////////////////////////////////////////////////////

function AQL_RELATIONAL_EQUAL (lhs, rhs) {
  // determines if two values are the same, returns a bool or undefined
  var leftWeight = AQL_TYPEWEIGHT(lhs);
  var rightWeight = AQL_TYPEWEIGHT(rhs);

  if (leftWeight === AQL_TYPEWEIGHT_UNDEFINED ||
      rightWeight === AQL_TYPEWEIGHT_UNDEFINED) {
    return undefined;
  }

  if (leftWeight != rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= AQL_TYPEWEIGHT_ARRAY) {
    // arrays and objects
    var l = AQL_KEYS(lhs);
    var r = AQL_KEYS(rhs);
    var numLeft = l.length;
    var numRight = r.length;

    if (numLeft !== numRight) {
      return false;
    }

    for (var i = 0; i < numLeft; ++i) {
      var key = l[i];
      if (key !== r[i]) {
        // keys must be identical
        return false;
      }

      var result = AQL_RELATIONAL_EQUAL(lhs[key], rhs[key]);
      if (result === undefined || result === false) {
        return result;
      }
    }
    return true;
  }

  // primitive type
  return (lhs === rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform unequality check 
////////////////////////////////////////////////////////////////////////////////

function AQL_RELATIONAL_UNEQUAL (lhs, rhs) {
  // determines if two values are not the same, returns a bool or undefined
  var leftWeight = AQL_TYPEWEIGHT(lhs);
  var rightWeight = AQL_TYPEWEIGHT(rhs);
  
  if (leftWeight === AQL_TYPEWEIGHT_UNDEFINED ||
      rightWeight === AQL_TYPEWEIGHT_UNDEFINED) {
    return undefined;
  }

  if (leftWeight != rightWeight) {
    return true;
  }

  // lhs and rhs have the same type

  if (leftWeight >= AQL_TYPEWEIGHT_ARRAY) {
    // arrays and objects
    var l = AQL_KEYS(lhs);
    var r = AQL_KEYS(rhs);
    var numLeft = l.length;
    var numRight = r.length;

    if (numLeft !== numRight) {
      return true;
    }

    for (var i = 0; i < numLeft; ++i) {
      var key = l[i];
      if (key !== r[i]) {
        // keys differ => unequality
        return true;
      }

      var result = AQL_RELATIONAL_UNEQUAL(lhs[key], rhs[key]);
      if (result === undefined || result === true) {
        return result;
      }
    }
    return false;
  }

  // primitive type
  return (lhs !== rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform greater than check (inner function)
////////////////////////////////////////////////////////////////////////////////

function AQL_RELATIONAL_GREATER_REC (lhs, rhs) {
  // determines if lhs is greater than rhs, returns a bool or undefined
  var leftWeight = AQL_TYPEWEIGHT(lhs);
  var rightWeight = AQL_TYPEWEIGHT(rhs);
  
  if (leftWeight === AQL_TYPEWEIGHT_UNDEFINED ||
      rightWeight === AQL_TYPEWEIGHT_UNDEFINED) {
    return undefined;
  }

  if (leftWeight > rightWeight) {
    return true;
  }
  if (leftWeight < rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= AQL_TYPEWEIGHT_ARRAY) {
    // arrays and objects
    var l = AQL_KEYS(lhs);
    var r = AQL_KEYS(rhs);
    var numLeft = l.length;
    var numRight = r.length;

    for (var i = 0; i < numLeft; ++i) {
      if (i >= numRight) {
        // right operand does not have any more keys
        return true;
      }
      var key = l[i];
      if (key < r[i]) {
        // left key is less than right key
        return false;
      } 
      else if (key > r[i]) {
        // left key is bigger than right key
        return true;
      }

      var result = AQL_RELATIONAL_GREATER_REC(lhs[i], rhs[i]);
      if (result !== null) {
        return result;
      }
    }
    return null;
  }

  // primitive type
  if (lhs === rhs) {
    return null;
  }
  return (lhs > rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform greater than check 
////////////////////////////////////////////////////////////////////////////////

function AQL_RELATIONAL_GREATER (lhs, rhs) {
  var result = AQL_RELATIONAL_GREATER_REC(lhs, rhs);

  if (result === null) {
    result = false;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform greater equal check 
////////////////////////////////////////////////////////////////////////////////

function AQL_RELATIONAL_GREATEREQUAL (lhs, rhs) {
  // determines if lhs is greater or equal to rhs, returns a bool or undefined
  var leftWeight = AQL_TYPEWEIGHT(lhs);
  var rightWeight = AQL_TYPEWEIGHT(rhs);
  
  if (leftWeight === AQL_TYPEWEIGHT_UNDEFINED ||
      rightWeight === AQL_TYPEWEIGHT_UNDEFINED) {
    return undefined;
  }

  if (leftWeight > rightWeight) {
    return true;
  }
  if (leftWeight < rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= AQL_TYPEWEIGHT_ARRAY) {
    // arrays and objects
    var l = AQL_KEYS(lhs);
    var r = AQL_KEYS(rhs);
    var numLeft = l.length;
    var numRight = r.length;

    for (var i = 0; i < numLeft; ++i) {
      if (i >= numRight) {
        return true;
      }
      var result = AQL_RELATIONAL_GREATEREQUAL(lhs[i], rhs[i]);
      if (result === undefined || result === false) {
        return result;
      }
    }
    return true;
  }

  // primitive type
  return (lhs >= rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform less than check (inner function)
////////////////////////////////////////////////////////////////////////////////

function AQL_RELATIONAL_LESS_REC (lhs, rhs) {
  // determines if lhs is less than rhs, returns a bool or undefined
  var leftWeight = AQL_TYPEWEIGHT(lhs);
  var rightWeight = AQL_TYPEWEIGHT(rhs);
  
  if (leftWeight === AQL_TYPEWEIGHT_UNDEFINED ||
      rightWeight === AQL_TYPEWEIGHT_UNDEFINED) {
    return undefined;
  }

  if (leftWeight < rightWeight) {
    return true;
  }
  if (leftWeight > rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= AQL_TYPEWEIGHT_ARRAY) {
    // arrays and objects
    var l = AQL_KEYS(lhs);
    var r = AQL_KEYS(rhs);
    var numLeft = l.length;
    var numRight = r.length;

    for (var i = 0; i < numRight; ++i) {
      if (i >= numLeft) {
        // left operand does not have any more keys
        return true;
      }
      var key = l[i];
      if (key < r[i]) {
        // left key is less than right key
        return true;
      } 
      else if (key > r[i]) {
        // left key is bigger than right key
        return false;
      }
      var result = AQL_RELATIONAL_LESS_REC(lhs[key], rhs[key]);
      if (result !== null) {
        return result;
      }
    }
    return null;
  }

  // primitive type
  if (lhs === rhs) {
    return null;
  }
  return (lhs < rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform less than check 
////////////////////////////////////////////////////////////////////////////////

function AQL_RELATIONAL_LESS (lhs, rhs) {
  var result = AQL_RELATIONAL_LESS_REC(lhs, rhs);

  if (result === null) {
    result = false;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform less equal check 
////////////////////////////////////////////////////////////////////////////////

function AQL_RELATIONAL_LESSEQUAL (lhs, rhs) {
  // determines if lhs is less or equal to rhs, returns a bool or undefined
  var leftWeight = AQL_TYPEWEIGHT(lhs);
  var rightWeight = AQL_TYPEWEIGHT(rhs);
  
  if (leftWeight === AQL_TYPEWEIGHT_UNDEFINED ||
      rightWeight === AQL_TYPEWEIGHT_UNDEFINED) {
    return undefined;
  }

  if (leftWeight < rightWeight) {
    return true;
  }
  if (leftWeight > rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= AQL_TYPEWEIGHT_ARRAY) {
    // arrays and objects
    var l = AQL_KEYS(lhs);
    var r = AQL_KEYS(rhs);
    var numLeft = l.length;
    var numRight = r.length;

    for (var i = 0; i < numRight; ++i) {
      if (i >= numLeft) {
        return true;
      }
      var result = AQL_RELATIONAL_LESSEQUAL(lhs[i], rhs[i]);
      if (result === undefined || result === false) {
        return result;
      }
    }
    return true;
  }

  // primitive type
  return (lhs <= rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform in list check 
////////////////////////////////////////////////////////////////////////////////

function AQL_RELATIONAL_IN (lhs, rhs) {
  // lhs IN rhs: both operands must have the same type, returns a bool or undefined
  var leftWeight = AQL_TYPEWEIGHT(lhs);
  var rightWeight = AQL_TYPEWEIGHT(rhs);
  
  if (leftWeight === AQL_TYPEWEIGHT_UNDEFINED ||
      rightWeight !== AQL_TYPEWEIGHT_ARRAY) {
    return undefined;
  }
  
  var r = AQL_KEYS(rhs);
  var numRight = r.length;

  for (var i = 0; i < numRight; ++i) {
    if (AQL_RELATIONAL_EQUAL(lhs, r[i])) {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform unary plus operation
////////////////////////////////////////////////////////////////////////////////

function AQL_UNARY_PLUS (lhs) {
  // +lhs: operand must be numeric, returns a number or undefined
  if (AQL_TYPEWEIGHT(lhs) !== AQL_TYPEWEIGHT_NUMBER) {
    return undefined;
  }

  return lhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform unary minus operation
////////////////////////////////////////////////////////////////////////////////

function AQL_UNARY_MINUS (lhs) {
  // -lhs: operand must be numeric, returns a number or undefined
  if (AQL_TYPEWEIGHT(lhs) !== AQL_TYPEWEIGHT_NUMBER) {
    return undefined;
  }

  return -lhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic plus
////////////////////////////////////////////////////////////////////////////////

function AQL_ARITHMETIC_PLUS (lhs, rhs) { 
  // lhs + rhs: operands must be numeric, returns a number or undefined
  if (AQL_TYPEWEIGHT(lhs) !== AQL_TYPEWEIGHT_NUMBER ||
      AQL_TYPEWEIGHT(rhs) !== AQL_TYPEWEIGHT_NUMBER) {
    return undefined;
  }

  return (lhs + rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic minus
////////////////////////////////////////////////////////////////////////////////

function AQL_ARITHMETIC_MINUS (lhs, rhs) {
  // lhs - rhs: operands must be numeric, returns a number or undefined
  if (AQL_TYPEWEIGHT(lhs) !== AQL_TYPEWEIGHT_NUMBER ||
      AQL_TYPEWEIGHT(rhs) !== AQL_TYPEWEIGHT_NUMBER) {
    return undefined;
  }

  return (lhs - rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic multiplication
////////////////////////////////////////////////////////////////////////////////

function AQL_ARITHMETIC_TIMES (lhs, rhs) {
  // lhs * rhs: operands must be numeric, returns a number or undefined
  if (AQL_TYPEWEIGHT(lhs) !== AQL_TYPEWEIGHT_NUMBER ||
      AQL_TYPEWEIGHT(rhs) !== AQL_TYPEWEIGHT_NUMBER) {
    return undefined;
  }

  return (lhs * rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic division
////////////////////////////////////////////////////////////////////////////////

function AQL_ARITHMETIC_DIVIDE (lhs, rhs) {
  // lhs / rhs: operands must be numeric, returns a number or undefined
  if (AQL_TYPEWEIGHT(lhs) !== AQL_TYPEWEIGHT_NUMBER ||
      AQL_TYPEWEIGHT(rhs) !== AQL_TYPEWEIGHT_NUMBER ||
      rhs == 0) {
    return undefined;
  }

  return (lhs / rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic modulus
////////////////////////////////////////////////////////////////////////////////

function AQL_ARITHMETIC_MODULUS (lhs, rhs) {
  // lhs % rhs: operands must be numeric, returns a number or undefined
  if (AQL_TYPEWEIGHT(lhs) !== AQL_TYPEWEIGHT_NUMBER ||
      AQL_TYPEWEIGHT(rhs) !== AQL_TYPEWEIGHT_NUMBER ||
      rhs == 0) {
    return undefined;
  }

  return (lhs % rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform string concatenation
////////////////////////////////////////////////////////////////////////////////

function AQL_STRING_CONCAT (lhs, rhs) {
  // string concatenation of lhs and rhs: operands must be strings, returns a string or undefined
  if (AQL_TYPEWEIGHT(lhs) !== AQL_TYPEWEIGHT_STRING ||
      AQL_TYPEWEIGHT(rhs) !== AQL_TYPEWEIGHT_STRING) {
    return undefined;
  }

  return (lhs + rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to a number
////////////////////////////////////////////////////////////////////////////////

function AQL_CAST_NUMBER (lhs) {
  // (number) lhs: operand can have any type, returns a number
  switch (AQL_TYPEWEIGHT(lhs)) {
    case AQL_TYPEWEIGHT_UNDEFINED:
    case AQL_TYPEWEIGHT_NULL:
    case AQL_TYPEWEIGHT_ARRAY:
    case AQL_TYPEWEIGHT_OBJECT:
      return 0.0;
    case AQL_TYPEWEIGHT_BOOL:
      return (lhs ? 1 : 0);
    case AQL_TYPEWEIGHT_NUMBER:
      return lhs;
    case AQL_TYPEWEIGHT_STRING:
      var result = parseFloat(lhs);
      return ((AQL_TYPEWEIGHT(result) === AQL_TYPEWEIGHT_NUMBER) ? result : 0);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to a string
////////////////////////////////////////////////////////////////////////////////

function AQL_CAST_STRING (lhs) {
  // (string) lhs: operand can have any type, returns a string
  switch (AQL_TYPEWEIGHT(lhs)) {
    case AQL_TYPEWEIGHT_STRING:
      return lhs;
    case AQL_TYPEWEIGHT_UNDEFINED:
      return 'undefined';
    case AQL_TYPEWEIGHT_NULL:
      return 'null';
    case AQL_TYPEWEIGHT_BOOL:
      return (lhs ? 'true' : 'false');
    case AQL_TYPEWEIGHT_NUMBER:
    case AQL_TYPEWEIGHT_ARRAY:
    case AQL_TYPEWEIGHT_OBJECT:
      return lhs.toString();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to a bool
////////////////////////////////////////////////////////////////////////////////

function AQL_CAST_BOOL (lhs) {
  // (bool) lhs: operand can have any type, returns a bool
  switch (AQL_TYPEWEIGHT(lhs)) {
    case AQL_TYPEWEIGHT_UNDEFINED:
    case AQL_TYPEWEIGHT_NULL:
      return false;
    case AQL_TYPEWEIGHT_BOOL:
      return lhs;
    case AQL_TYPEWEIGHT_NUMBER:
      return (lhs != 0);
    case AQL_TYPEWEIGHT_STRING: 
      return (lhs != '');
    case AQL_TYPEWEIGHT_ARRAY:
      return (lhs.length > 0);
    case AQL_TYPEWEIGHT_OBJECT:
      return (AQL_KEYS(lhs).length > 0);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to null
////////////////////////////////////////////////////////////////////////////////

function AQL_CAST_NULL (lhs) {
  // (null) lhs: operand can have any type, always returns null
  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to undefined
////////////////////////////////////////////////////////////////////////////////

function AQL_CAST_UNDEFINED (lhs) {
  // (null) lhs: operand can have any type, always returns undefined
  return undefined;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type string
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_STRING (lhs) {
  return (AQL_TYPEWEIGHT(lhs) === AQL_TYPEWEIGHT_STRING);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type number
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_NUMBER (lhs) {
  return (AQL_TYPEWEIGHT(lhs) === AQL_TYPEWEIGHT_NUMBER);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type bool
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_BOOL (lhs) {
  return (AQL_TYPEWEIGHT(lhs) === AQL_TYPEWEIGHT_BOOL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type null
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_NULL (lhs) {
  return (AQL_TYPEWEIGHT(lhs) === AQL_TYPEWEIGHT_NULL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type undefined
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_UNDEFINED (lhs) {
  return (AQL_TYPEWEIGHT(lhs) === AQL_TYPEWEIGHT_UNDEFINED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type array
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_ARRAY (lhs) {
  return (AQL_TYPEWEIGHT(lhs) === AQL_TYPEWEIGHT_ARRAY);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type object
////////////////////////////////////////////////////////////////////////////////

function AQL_IS_OBJECT (lhs) {
  return (AQL_TYPEWEIGHT(lhs) === AQL_TYPEWEIGHT_OBJECT);
}


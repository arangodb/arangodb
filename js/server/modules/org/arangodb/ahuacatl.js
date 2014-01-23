/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, continue: true */
/*global require, exports, COMPARE_STRING, MATCHES */

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

var INTERNAL = require("internal");
var TRAVERSAL = require("org/arangodb/graph/traversal");
var ArangoError = require("org/arangodb").ArangoError;
var ShapedJson = INTERNAL.ShapedJson;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief cache for compiled regexes
////////////////////////////////////////////////////////////////////////////////

var RegexCache = { };

////////////////////////////////////////////////////////////////////////////////
/// @brief user functions cache
////////////////////////////////////////////////////////////////////////////////

var UserFunctions = { };

////////////////////////////////////////////////////////////////////////////////
/// @brief type weight used for sorting and comparing
////////////////////////////////////////////////////////////////////////////////

var TYPEWEIGHT_NULL      = 0;
var TYPEWEIGHT_BOOL      = 1;
var TYPEWEIGHT_NUMBER    = 2;
var TYPEWEIGHT_STRING    = 4;
var TYPEWEIGHT_LIST      = 8;
var TYPEWEIGHT_DOCUMENT  = 16;

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief throw a runtime exception
////////////////////////////////////////////////////////////////////////////////

function THROW (error, data) {
  "use strict";

  var err = new ArangoError();

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
/// @brief return a database-specific function prefix
////////////////////////////////////////////////////////////////////////////////

function DB_PREFIX () {
  return INTERNAL.db._name();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reset the regex cache
////////////////////////////////////////////////////////////////////////////////

function resetRegexCache () {
  "use strict";

  RegexCache = { 'i' : { }, '' : { } };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reset the user functions and reload them from the database
////////////////////////////////////////////////////////////////////////////////

function reloadUserFunctions () {
  "use strict";

  var c;

  c = INTERNAL.db._collection("_aqlfunctions");

  if (c === null) {
    return;
  }

  var foundError = false;
  var prefix = DB_PREFIX();

  UserFunctions[prefix] = { };

  c.toArray().forEach(function (f) {
    var code = "(function() { var callback = " + f.code + "; return callback; })();";
    var key = f._key.replace(/:{1,}/g, '::');

    try {
      var res = INTERNAL.executeScript(code, undefined, "(user function " + key + ")"); 

      UserFunctions[prefix][key.toUpperCase()] = {
        name: key,
        func: res,
        isDeterministic: f.isDeterministic || false
      }; 
  
    }
    catch (err) {
      foundError = true;
    }
  });
      
  if (foundError) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_INVALID_CODE);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reset the query engine
////////////////////////////////////////////////////////////////////////////////

function resetEngine () {
  "use strict";

  resetRegexCache();
  reloadUserFunctions();
}


////////////////////////////////////////////////////////////////////////////////
/// @brief normalise a function name
////////////////////////////////////////////////////////////////////////////////

function NORMALIZE_FNAME (functionName) {
  "use strict";

  var p = functionName.indexOf('::');

  if (p === -1) {
    return functionName;
  }

  return functionName.substr(p + 2);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief filter using a list of examples
////////////////////////////////////////////////////////////////////////////////

function FILTER (list, examples) {
  "use strict";

  var result = [ ], i;

  if (examples === undefined || examples === null) {
    return list;
  }

  for (i = 0; i < list.length; ++i) {
    var element = list[i];

    if (MATCHES(element, examples, false)) {
      result.push(element);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find a fulltext index for a certain attribute & collection
////////////////////////////////////////////////////////////////////////////////

function INDEX_FULLTEXT (collection, attribute) {
  "use strict";

  var indexes = collection.getIndexes(), i;

  for (i = 0; i < indexes.length; ++i) {
    var index = indexes[i];
    if (index.type === "fulltext" && index.fields && index.fields[0] === attribute) {
      return index.id;
    }
  }

  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find an index of a certain type for a collection
////////////////////////////////////////////////////////////////////////////////

function INDEX (collection, indexTypes) {
  "use strict";

  var indexes = collection.getIndexes(), i, j;

  for (i = 0; i < indexes.length; ++i) {
    var index = indexes[i];

    for (j = 0; j < indexTypes.length; ++j) {
      if (index.type === indexTypes[j]) {
        return index.id;
      }
    }
  }

  return null;
}
 
////////////////////////////////////////////////////////////////////////////////
/// @brief get access to a collection
////////////////////////////////////////////////////////////////////////////////

function COLLECTION (name) {
  "use strict";

  if (typeof name !== 'string') {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "INTERNAL");
  }

  if (name.substring(0, 1) === '_') {
    // system collections need to be accessed slightly differently as they
    // are not returned by the propertyGetter of db
    return INTERNAL.db._collection(name);
  }

  return INTERNAL.db[name];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone an object
////////////////////////////////////////////////////////////////////////////////

function CLONE (obj) {
  "use strict";

  if (obj === null || typeof(obj) !== "object") {
    return obj;
  }
 
  var copy, a; 
  if (Array.isArray(obj)) {
    copy = [ ];
    obj.forEach(function (i) {
      copy.push(CLONE(i));
    });
  }
  else if (obj instanceof Object) {
    copy = { };
    for (a in obj) {
      if (obj.hasOwnProperty(a)) {
        copy[a] = CLONE(obj[a]);
      }
    }
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief box a value into the AQL datatype system
////////////////////////////////////////////////////////////////////////////////

function FIX_VALUE (value) {
  "use strict";
  
  if (value instanceof ShapedJson) {
    return value;
  }

  var type = typeof(value), i;

  if (value === undefined || 
      value === null || 
      (type === 'number' && (isNaN(value) || ! isFinite(value)))) {
    return null;
  }
  
  if (type === 'boolean' || type === 'string' || type === 'number') {
    return value;
  }

  if (Array.isArray(value)) {
    for (i = 0; i < value.length; ++i) {
      value[i] = FIX_VALUE(value[i]);
    }

    return value;
  }

  if (type === 'object') {
    var result = { };
    for (i in value) {
      if (value.hasOwnProperty(i)) {
        if (typeof value[i] === 'function') {
          continue;
        }
        result[i] = FIX_VALUE(value[i]);
      }
    }

    return result;
  }

  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the sort type of an operand
////////////////////////////////////////////////////////////////////////////////

function TYPEWEIGHT (value) {
  "use strict";

  if (value === undefined || value === null) {
    return TYPEWEIGHT_NULL;
  }

  if (Array.isArray(value)) {
    return TYPEWEIGHT_LIST;
  }

  switch (typeof(value)) {
    case 'boolean':
      return TYPEWEIGHT_BOOL;
    case 'number':
      if (isNaN(value) || ! isFinite(value)) {
        // not a number => undefined
        return TYPEWEIGHT_NULL; 
      }
      return TYPEWEIGHT_NUMBER;
    case 'string':
      return TYPEWEIGHT_STRING;
    case 'object':
      return TYPEWEIGHT_DOCUMENT;
  }

  return TYPEWEIGHT_NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate function call argument
////////////////////////////////////////////////////////////////////////////////

function ARG_CHECK (actualValue, expectedType, functionName) {
  "use strict";

  if (TYPEWEIGHT(actualValue) !== expectedType) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, 
          NORMALIZE_FNAME(functionName));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compile a regex from a string pattern
////////////////////////////////////////////////////////////////////////////////

function COMPILE_REGEX (regex, modifiers) {
  "use strict";

  var i, n = regex.length;
  var escaped = false;
  var pattern = '';
  var specialChar = /^([.*+?\^=!:${}()|\[\]\/\\])$/;
  
  ARG_CHECK(regex, TYPEWEIGHT_STRING, "LIKE");

  for (i = 0; i < n; ++i) {
    var c = regex.charAt(i);

    if (c === '\\') {
      if (escaped) {
        // literal \
        pattern += '\\\\';
      }
      escaped = ! escaped;
    }
    else {
      if (c === '%') {
        if (escaped) {
          // literal %
          pattern += '%';
        }
        else {
          // wildcard
          pattern += '.*';
        }
      }
      else if (c === '_') {
        if (escaped) {
          // literal _
          pattern += '_';
        }
        else {
          // wildcard character
          pattern += '.';
        }
      }
      else if (c.match(specialChar)) {
        // character with special meaning in a regex
        pattern += '\\' + c;
      }
      else {
        // literal character
        pattern += c;
      }

      escaped = false;
    }
  }
 
  return new RegExp('^' + pattern + '$', modifiers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief call a function
////////////////////////////////////////////////////////////////////////////////

function FCALL (name, parameters) {
  "use strict";

  return name.apply(null, parameters);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief call a user function
////////////////////////////////////////////////////////////////////////////////

function FCALL_USER (name, parameters) {
  "use strict";

  var prefix = DB_PREFIX();
  if (! UserFunctions.hasOwnProperty(prefix)) {
    reloadUserFunctions();

    if (! UserFunctions.hasOwnProperty(prefix)) {
      THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, NORMALIZE_FNAME(name));
    }
  }

  if (UserFunctions[prefix].hasOwnProperty(name)) {
    var result = UserFunctions[prefix][name].func.apply(null, parameters);

    return FIX_VALUE(result);
  }

  THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_NOT_FOUND, NORMALIZE_FNAME(name));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the numeric value or undefined if it is out of range
////////////////////////////////////////////////////////////////////////////////

function NUMERIC_VALUE (value) {
  "use strict";

  if (isNaN(value) || ! isFinite(value)) {
    return null;
  }

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fix a value for a comparison
////////////////////////////////////////////////////////////////////////////////

function FIX (value) {
  "use strict";

  if (value === undefined) {
    return null;
  }

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the values of an object in the order that they are defined
////////////////////////////////////////////////////////////////////////////////

function VALUES (value) {
  "use strict";

  var values = [ ], a;
  
  for (a in value) {
    if (value.hasOwnProperty(a)) {
      values.push(value[a]);
    }
  }

  return values;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract key names from an argument list
////////////////////////////////////////////////////////////////////////////////

function EXTRACT_KEYS (args, startArgument, functionName) {
  "use strict";

  var keys = { }, i, j, key, key2;

  for (i = startArgument; i < args.length; ++i) {
    key = args[i];
    if (typeof key === 'string') {
      keys[key] = true;
    }
    else if (Array.isArray(key)) {
      for (j = 0; j < key.length; ++j) {
        key2 = key[j];
        if (typeof key2 === 'string') {
          keys[key2] = true;
        }
        else {
          THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, 
                NORMALIZE_FNAME(functionName));
        }
      }
    }
  }

  return keys;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the keys of an array or object in a comparable way
////////////////////////////////////////////////////////////////////////////////

function KEYS (value, doSort) {
  "use strict";

  var keys;
  
  if (Array.isArray(value)) {
    var n = value.length, i;
    keys = [ ];

    for (i = 0; i < n; ++i) {
      keys.push(i);
    }
  }
  else {
    keys = Object.keys(value);

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

function KEYLIST (lhs, rhs) {
  "use strict";

  if (Array.isArray(lhs)) {
    // lhs & rhs are lists
    return KEYS(lhs.length > rhs.length ? lhs : rhs);
  }
  
  // lhs & rhs are arrays
  var a, keys = KEYS(lhs);
  for (a in rhs) {
    if (rhs.hasOwnProperty(a) && ! lhs.hasOwnProperty(a)) {
      keys.push(a);
    }
  }

  // object keys need to be sorted by names
  keys.sort();

  return keys;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an indexed value from an array or document (e.g. users[3])
////////////////////////////////////////////////////////////////////////////////

function GET_INDEX (value, index) {
  "use strict";

  if (TYPEWEIGHT(value) === TYPEWEIGHT_NULL) {
    return null;
  }
  
  var result = null;
  if (TYPEWEIGHT(value) === TYPEWEIGHT_DOCUMENT) {
    result = value[index];
  }
  else if (TYPEWEIGHT(value) === TYPEWEIGHT_LIST) {
    if (index < 0) {
      // negative indexes fetch the element from the end, e.g. -1 => value[value.length - 1];
      index = value.length + index;
    }

    if (index >= 0) {
      result = value[index];
    }
  }
  else {
    THROW(INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
  }

  if (TYPEWEIGHT(result) === TYPEWEIGHT_NULL) {
    return null;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize a value for comparison, sorting etc.
////////////////////////////////////////////////////////////////////////////////

function NORMALIZE (value) {
  "use strict";

  if (value === null || value === undefined) {
    return null;
  }

  if (typeof(value) !== "object") {
    return value;
  }

  var result;

  if (Array.isArray(value)) {
    result = [ ];
    value.forEach(function (v) {
      result.push(NORMALIZE(v));
    });
  } 
  else {
    result = { };
    KEYS(value, true).forEach(function (a) {
      result[a] = NORMALIZE(value[a]);
    });
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an attribute from a document (e.g. users.name)
////////////////////////////////////////////////////////////////////////////////

function DOCUMENT_MEMBER (value, attributeName) {
  "use strict";

  if (TYPEWEIGHT(value) === TYPEWEIGHT_NULL) {
    return null;
  }

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_DOCUMENT) {
    return null;
  }

  var result = value[attributeName];

  if (TYPEWEIGHT(result) === TYPEWEIGHT_NULL) {
    return null;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief assert that a value is a list, fail otherwise
////////////////////////////////////////////////////////////////////////////////

function LIST (value) {
  "use strict";

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_LIST) {
    THROW(INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
  }

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a document by its unique id or their unique ids
////////////////////////////////////////////////////////////////////////////////

function DOCUMENT_HANDLE (id) {
  "use strict";

  if (TYPEWEIGHT(id) === TYPEWEIGHT_LIST) {
    var result = [ ], i;
    for (i = 0; i < id.length; ++i) {
      try {
        result.push(INTERNAL.db._document(id[i]));
      }
      catch (e1) {
      }
    }
    return result;
  }

  try {
    return INTERNAL.db._document(id);
  }
  catch (e2) {
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a document by its unique id or their unique ids
////////////////////////////////////////////////////////////////////////////////

function DOCUMENT (collection, id) {
  "use strict";

  // we're polymorphic
  if (id === undefined) {
    // called with a single parameter
    var weight = TYPEWEIGHT(collection);

    if (weight === TYPEWEIGHT_STRING || weight === TYPEWEIGHT_LIST) {
      return DOCUMENT_HANDLE(collection);
    }
  }

  if (TYPEWEIGHT(id) === TYPEWEIGHT_LIST) {
    var c = COLLECTION(collection);

    var result = [ ], i;
    for (i = 0; i < id.length; ++i) {
      try {
        result.push(c.document(id[i]));
      }
      catch (e1) {
      }
    }
    return result;
  }

  try {
    return COLLECTION(collection).document(id);
  }
  catch (e2) {
    return null;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get all documents from the specified collection
////////////////////////////////////////////////////////////////////////////////

function GET_DOCUMENTS (collection, offset, limit) {
  "use strict";

  if (offset === undefined) {
    offset = 0;
  }
  if (limit === undefined) {
    limit = null;
  }
  return COLLECTION(collection).ALL(offset, limit).documents;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get all documents from the specified collection, incrementally
/// this is the init function for incremental access
////////////////////////////////////////////////////////////////////////////////

function GET_DOCUMENTS_INCREMENTAL_INIT (collection, offset, limit) {
  "use strict";

  if (offset === undefined) {
    offset = 0;
  }
  if (limit === undefined) {
    limit = null;
  }

  var batchSize = 2000;
  var c = COLLECTION(collection);
  var state = c.OFFSET(0, batchSize, offset, null);
  state.collection = c;
  state.batchSize  = batchSize;
  state.offset     = 0;
  state.limit      = limit;
  state.length     = state.total - offset;

  if (state.limit !== null && state.limit < state.length) {
    state.length = state.limit; 
  }
  
  state.remain = state.length - state.documents.length;
  if (state.remain < 0) {
    state.remain = 0;
  }
 
  return state;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get all documents from the specified collection, incrementally
/// this is the continuation function for incremental access
////////////////////////////////////////////////////////////////////////////////

function GET_DOCUMENTS_INCREMENTAL_CONT (oldState) {
  "use strict";

  var state = oldState.collection.OFFSET(oldState.skip, oldState.batchSize, 0, null);
  state.collection = oldState.collection;
  state.batchSize = oldState.batchSize;
  state.offset = oldState.offset + oldState.batchSize;
  state.limit = oldState.limit;
  state.length = oldState.length;
  state.remain = oldState.remain;
  
  if (state.documents.length > state.remain) {
    state.documents = state.documents.slice(0, state.remain);
  }

  state.remain -= state.documents.length;
  return state;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get documents from the specified collection using the primary index
/// (single index value)
////////////////////////////////////////////////////////////////////////////////

function GET_DOCUMENTS_PRIMARY (collection, idx, id) {
  "use strict";

  try {
    return [ COLLECTION(collection).document(id) ];
  }
  catch (e) {
    return [ ];
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get documents from the specified collection using the primary index
/// (multiple index values)
////////////////////////////////////////////////////////////////////////////////

function GET_DOCUMENTS_PRIMARY_LIST (collection, idx, values) {
  "use strict";

  var result = [ ], c;

  c = COLLECTION(collection);

  values.forEach (function (id) {
    try {
      result.push(c.document(id));
    }
    catch (e) {
    }
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get documents from the specified collection using a hash index
/// (single index value)
////////////////////////////////////////////////////////////////////////////////

function GET_DOCUMENTS_HASH (collection, idx, example) {
  "use strict";

  return COLLECTION(collection).BY_EXAMPLE_HASH(idx, example).documents;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get documents from the specified collection using a hash index
/// (multiple index values)
////////////////////////////////////////////////////////////////////////////////

function GET_DOCUMENTS_HASH_LIST (collection, idx, attribute, values) {
  "use strict";

  var result = [ ], c;
  
  c = COLLECTION(collection);

  values.forEach(function (value) {
    var example = { };

    example[attribute] = value;

    c.BY_EXAMPLE_HASH(idx, example).documents.forEach(function (doc) {
      result.push(doc);
    });
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get documents from the specified collection using the edge index
/// (single index value)
////////////////////////////////////////////////////////////////////////////////

function GET_DOCUMENTS_EDGE (collection, att, id) {
  "use strict";

  var result;

  try {
    if (att === '_from') {
      result = COLLECTION(collection).outEdges(id);
    }
    else {
      result = COLLECTION(collection).inEdges(id);
    }
  }
  catch (e) {
    result = [ ];
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get documents from the specified collection using the edge index
/// (multiple index values)
////////////////////////////////////////////////////////////////////////////////

function GET_DOCUMENTS_EDGE_LIST (collection, att, values) {
  "use strict";

  var docs = { }, result = [ ], c, a;

  c = COLLECTION(collection);

  values.forEach(function (value) {
    try {
      var parts;
      if (att === '_from') {
        parts = c.outEdges(value);
      }
      else {
        parts = c.inEdges(value);
      }

      parts.forEach(function (e) {
        docs[e._id] = e;
      });
    }
    catch (e) {
    }
  });

  for (a in docs) {
    if (docs.hasOwnProperty(a)) {
      result.push(docs[a]);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get documents from the specified collection using a bitarray
////////////////////////////////////////////////////////////////////////////////

function GET_DOCUMENTS_BITARRAY (collection, idx, example) {
  "use strict";

  return COLLECTION(collection).BY_CONDITION_BITARRAY(idx, example).documents;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get documents from the specified collection using a bitarray
/// (multiple index values) TODO: replace by 'IN index operator'
////////////////////////////////////////////////////////////////////////////////

function GET_DOCUMENTS_BITARRAY_LIST (collection, idx, attribute, values) {
  "use strict";

  var result = [ ], c;

  c = COLLECTION(collection);

  values.forEach(function (value) {
    var example = { }, documents;

    example[attribute] = value;

    documents = c.BY_EXAMPLE_BITARRAY(idx, example).documents;
    documents.forEach(function (doc) {
      result.push(doc);
    });
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get documents from the specified collection using a skiplist
////////////////////////////////////////////////////////////////////////////////

function GET_DOCUMENTS_SKIPLIST (collection, idx, example) {
  "use strict";

  return COLLECTION(collection).BY_CONDITION_SKIPLIST(idx, example).documents;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get documents from the specified collection using a skiplist
/// (multiple index values)
////////////////////////////////////////////////////////////////////////////////

function GET_DOCUMENTS_SKIPLIST_LIST (collection, idx, attribute, values) {
  "use strict";

  var result = [ ], c = COLLECTION(collection);

  values.forEach(function (value) {
    var example = { }, documents;

    example[attribute] = value;
    documents = c.BY_EXAMPLE_SKIPLIST(idx, example).documents;
    documents.forEach(function (doc) {
      result.push(doc);
    });
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get names of all collections
////////////////////////////////////////////////////////////////////////////////

function COLLECTIONS () {
  "use strict";

  var result = [ ];
  
  INTERNAL.db._collections().forEach(function (c) {
    result.push({ 
      _id : c._id, 
      name : c.name() 
    });
  });

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                logical operations
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief execute ternary operator
///
/// the condition operand must be a boolean value, returns either the truepart
/// or the falsepart 
////////////////////////////////////////////////////////////////////////////////

function TERNARY_OPERATOR (condition, truePart, falsePart) {
  "use strict";

  if (TYPEWEIGHT(condition) !== TYPEWEIGHT_BOOL) {
    THROW(INTERNAL.errors.ERROR_QUERY_INVALID_LOGICAL_VALUE);
  }

  if (condition) {
    return truePart;
  }
  return falsePart;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute ternary operator
///
/// the condition operand must be a boolean value, returns either the truepart
/// or the falsepart 
////////////////////////////////////////////////////////////////////////////////

function TERNARY_OPERATOR_FN (condition, truePart, falsePart) {
  "use strict";

  if (TYPEWEIGHT(condition) !== TYPEWEIGHT_BOOL) {
    THROW(INTERNAL.errors.ERROR_QUERY_INVALID_LOGICAL_VALUE);
  }

  if (condition) {
    return truePart();
  }
  return falsePart();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform logical and
///
/// both operands must be boolean values, returns a boolean, uses short-circuit
/// evaluation
////////////////////////////////////////////////////////////////////////////////

function LOGICAL_AND (lhs, rhs) {
  "use strict";

  if (TYPEWEIGHT(lhs) !== TYPEWEIGHT_BOOL ||
      TYPEWEIGHT(rhs) !== TYPEWEIGHT_BOOL) {
    THROW(INTERNAL.errors.ERROR_QUERY_INVALID_LOGICAL_VALUE);
  }

  if (! lhs) {
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

function LOGICAL_OR (lhs, rhs) {
  "use strict";

  if (TYPEWEIGHT(lhs) !== TYPEWEIGHT_BOOL ||
      TYPEWEIGHT(rhs) !== TYPEWEIGHT_BOOL) {
    THROW(INTERNAL.errors.ERROR_QUERY_INVALID_LOGICAL_VALUE);
  }
  
  if (lhs) {
    return true;
  }

  return rhs;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform logical and
///
/// both operands must be boolean values, returns a boolean, uses short-circuit
/// evaluation
////////////////////////////////////////////////////////////////////////////////

function LOGICAL_AND_FN (lhs, rhs) {
  "use strict";

  var l = lhs();
  if (TYPEWEIGHT(l) !== TYPEWEIGHT_BOOL) {
    THROW(INTERNAL.errors.ERROR_QUERY_INVALID_LOGICAL_VALUE);
  }

  if (! l) {
    return false;
  }

  var r = rhs();
  if (TYPEWEIGHT(r) !== TYPEWEIGHT_BOOL) {
    THROW(INTERNAL.errors.ERROR_QUERY_INVALID_LOGICAL_VALUE);
  }

  return r;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform logical or
///
/// both operands must be boolean values, returns a boolean, uses short-circuit
/// evaluation
////////////////////////////////////////////////////////////////////////////////

function LOGICAL_OR_FN (lhs, rhs) {
  "use strict";

  var l = lhs();
  if (TYPEWEIGHT(l) !== TYPEWEIGHT_BOOL) {
    THROW(INTERNAL.errors.ERROR_QUERY_INVALID_LOGICAL_VALUE);
  }

  if (l) {
    return true;
  }

  var r = rhs();
  if (TYPEWEIGHT(r) !== TYPEWEIGHT_BOOL) {
    THROW(INTERNAL.errors.ERROR_QUERY_INVALID_LOGICAL_VALUE);
  }
  
  return r;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform logical negation
///
/// the operand must be a boolean values, returns a boolean
////////////////////////////////////////////////////////////////////////////////

function LOGICAL_NOT (lhs) {
  "use strict";

  if (TYPEWEIGHT(lhs) !== TYPEWEIGHT_BOOL) {
    THROW(INTERNAL.errors.ERROR_QUERY_INVALID_LOGICAL_VALUE);
  }

  return ! lhs;
}

// -----------------------------------------------------------------------------
// --SECTION--                                             comparison operations
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief perform equality check 
///
/// returns true if the operands are equal, false otherwise
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_EQUAL (lhs, rhs) {
  "use strict";

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);

  if (leftWeight !== rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = KEYLIST(lhs, rhs), i, n = keys.length; 
    for (i = 0; i < n; ++i) {
      var key = keys[i];
      if (RELATIONAL_EQUAL(lhs[key], rhs[key]) === false) {
        return false;
      }
    }
    return true;
  }

  // primitive type
  if (TYPEWEIGHT(lhs) === TYPEWEIGHT_NULL) {
    lhs = null;
  }
  if (TYPEWEIGHT(rhs) === TYPEWEIGHT_NULL) {
    rhs = null;
  }

  if (leftWeight === TYPEWEIGHT_STRING) {
    return COMPARE_STRING(lhs, rhs) === 0;
  }

  return (lhs === rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform inequality check 
///
/// returns true if the operands are unequal, false otherwise
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_UNEQUAL (lhs, rhs) {
  "use strict";

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);
  
  if (leftWeight !== rightWeight) {
    return true;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = KEYLIST(lhs, rhs), i, n = keys.length;
    for (i = 0; i < n; ++i) {
      var key = keys[i];
      if (RELATIONAL_UNEQUAL(lhs[key], rhs[key]) === true) {
        return true;
      }
    }

    return false;
  }

  // primitive type
  if (TYPEWEIGHT(lhs) === TYPEWEIGHT_NULL) {
    lhs = null;
  }
  if (TYPEWEIGHT(rhs) === TYPEWEIGHT_NULL) {
    rhs = null;
  }

  if (leftWeight === TYPEWEIGHT_STRING) {
    return COMPARE_STRING(lhs, rhs) !== 0;
  }

  return (lhs !== rhs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform greater than check (inner function)
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_GREATER_REC (lhs, rhs) {
  "use strict";

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);
  
  if (leftWeight > rightWeight) {
    return true;
  }
  if (leftWeight < rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = KEYLIST(lhs, rhs), i, n = keys.length;
    for (i = 0; i < n; ++i) {
      var key = keys[i], result = RELATIONAL_GREATER_REC(lhs[key], rhs[key]);
      if (result !== null) {
        return result;
      }
    }
    
    return null;
  }

  // primitive type
  if (TYPEWEIGHT(lhs) === TYPEWEIGHT_NULL) {
    lhs = null;
  }
  if (TYPEWEIGHT(rhs) === TYPEWEIGHT_NULL) {
    rhs = null;
  }

  if (leftWeight === TYPEWEIGHT_STRING) {
    return COMPARE_STRING(lhs, rhs) > 0;
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

function RELATIONAL_GREATER (lhs, rhs) {
  "use strict";

  var result = RELATIONAL_GREATER_REC(lhs, rhs);

  if (result === null) {
    result = false;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform greater equal check (inner function)
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_GREATEREQUAL_REC (lhs, rhs) {
  "use strict";

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);
  
  if (leftWeight > rightWeight) {
    return true;
  }
  if (leftWeight < rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = KEYLIST(lhs, rhs), i, n = keys.length;
    for (i = 0; i < n; ++i) {
      var key = keys[i], result = RELATIONAL_GREATEREQUAL_REC(lhs[key], rhs[key]);
      if (result !== null) {
        return result;
      }
    }
    
    return null;
  }

  // primitive type
  if (TYPEWEIGHT(lhs) === TYPEWEIGHT_NULL) {
    lhs = null;
  }
  if (TYPEWEIGHT(rhs) === TYPEWEIGHT_NULL) {
    rhs = null;
  }

  if (leftWeight === TYPEWEIGHT_STRING) {
    return COMPARE_STRING(lhs, rhs) >= 0;
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

function RELATIONAL_GREATEREQUAL (lhs, rhs) {
  "use strict";

  var result = RELATIONAL_GREATEREQUAL_REC(lhs, rhs);

  if (result === null) {
    result = true;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform less than check (inner function)
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_LESS_REC (lhs, rhs) {
  "use strict";
  
  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);
  
  if (leftWeight < rightWeight) {
    return true;
  }
  if (leftWeight > rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = KEYLIST(lhs, rhs), i, n = keys.length;
    for (i = 0; i < n; ++i) {
      var key = keys[i], result = RELATIONAL_LESS_REC(lhs[key], rhs[key]);
      if (result !== null) {
        return result;
      }
    }
    
    return null;
  }

  // primitive type
  if (TYPEWEIGHT(lhs) === TYPEWEIGHT_NULL) {
    lhs = null;
  }
  if (TYPEWEIGHT(rhs) === TYPEWEIGHT_NULL) {
    rhs = null;
  }

  if (leftWeight === TYPEWEIGHT_STRING) {
    return COMPARE_STRING(lhs, rhs) < 0;
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

function RELATIONAL_LESS (lhs, rhs) {
  "use strict";

  var result = RELATIONAL_LESS_REC(lhs, rhs);

  if (result === null) {
    result = false;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform less equal check (inner function)
////////////////////////////////////////////////////////////////////////////////

function RELATIONAL_LESSEQUAL_REC (lhs, rhs) {
  "use strict";

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);
  
  if (leftWeight < rightWeight) {
    return true;
  }
  if (leftWeight > rightWeight) {
    return false;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = KEYLIST(lhs, rhs), i, n = keys.length;
    for (i = 0; i < n; ++i) {
      var key = keys[i], result = RELATIONAL_LESSEQUAL_REC(lhs[key], rhs[key]);
      if (result !== null) {
        return result;
      }
    }

    return null;
  }
  
  // primitive type
  if (TYPEWEIGHT(lhs) === TYPEWEIGHT_NULL) {
    lhs = null;
  }
  if (TYPEWEIGHT(rhs) === TYPEWEIGHT_NULL) {
    rhs = null;
  }
  
  if (leftWeight === TYPEWEIGHT_STRING) {
    return COMPARE_STRING(lhs, rhs) <= 0;
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

function RELATIONAL_LESSEQUAL (lhs, rhs) {
  "use strict";

  var result = RELATIONAL_LESSEQUAL_REC(lhs, rhs);

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

function RELATIONAL_CMP (lhs, rhs) {
  "use strict";

  var leftWeight = TYPEWEIGHT(lhs);
  var rightWeight = TYPEWEIGHT(rhs);
  
  if (leftWeight < rightWeight) {
    return -1;
  }
  if (leftWeight > rightWeight) {
    return 1;
  }

  // lhs and rhs have the same type

  if (leftWeight >= TYPEWEIGHT_LIST) {
    // arrays and objects
    var keys = KEYLIST(lhs, rhs), i, n = keys.length;
    for (i = 0; i < n; ++i) {
      var key = keys[i], result = RELATIONAL_CMP(lhs[key], rhs[key]);
      if (result !== 0) {
        return result;
      }
    }
    
    return 0;
  }

  // primitive type
  if (TYPEWEIGHT(lhs) === TYPEWEIGHT_NULL) {
    lhs = null;
  }
  if (TYPEWEIGHT(rhs) === TYPEWEIGHT_NULL) {
    rhs = null;
  }

  if (leftWeight === TYPEWEIGHT_STRING) {
    return COMPARE_STRING(lhs, rhs);
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

function RELATIONAL_IN (lhs, rhs) {
  "use strict";

  var rightWeight = TYPEWEIGHT(rhs);
  
  if (rightWeight !== TYPEWEIGHT_LIST) {
    THROW(INTERNAL.errors.ERROR_QUERY_LIST_EXPECTED);
  }

  var numRight = rhs.length, i;
  for (i = 0; i < numRight; ++i) {
    if (RELATIONAL_EQUAL(lhs, rhs[i])) {
      return true;
    }
  }

  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                             arithmetic operations
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief perform unary plus operation
///
/// the operand must be numeric or this function will fail
////////////////////////////////////////////////////////////////////////////////

function UNARY_PLUS (value) {
  "use strict";

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_NUMBER) {
    THROW(INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
  }

  var result = NUMERIC_VALUE(value);
  if (TYPEWEIGHT(result) !== TYPEWEIGHT_NUMBER) {
    THROW(INTERNAL.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform unary minus operation
///
/// the operand must be numeric or this function will fail
////////////////////////////////////////////////////////////////////////////////

function UNARY_MINUS (value) {
  "use strict";

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_NUMBER) {
    THROW(INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
  }

  var result = NUMERIC_VALUE(-value);
  if (TYPEWEIGHT(result) !== TYPEWEIGHT_NUMBER) {
    THROW(INTERNAL.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic plus
///
/// both operands must be numeric or this function will fail
////////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_PLUS (lhs, rhs) { 
  "use strict";

  if (TYPEWEIGHT(lhs) !== TYPEWEIGHT_NUMBER ||
      TYPEWEIGHT(rhs) !== TYPEWEIGHT_NUMBER) {
    THROW(INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
  }

  var result = NUMERIC_VALUE(lhs + rhs);
  if (TYPEWEIGHT(result) !== TYPEWEIGHT_NUMBER) {
    THROW(INTERNAL.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic minus
///
/// both operands must be numeric or this function will fail
////////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_MINUS (lhs, rhs) {
  "use strict";

  if (TYPEWEIGHT(lhs) !== TYPEWEIGHT_NUMBER ||
      TYPEWEIGHT(rhs) !== TYPEWEIGHT_NUMBER) {
    THROW(INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
  }

  var result = NUMERIC_VALUE(lhs - rhs);
  if (TYPEWEIGHT(result) !== TYPEWEIGHT_NUMBER) {
    THROW(INTERNAL.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic multiplication
///
/// both operands must be numeric or this function will fail
////////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_TIMES (lhs, rhs) {
  "use strict";

  if (TYPEWEIGHT(lhs) !== TYPEWEIGHT_NUMBER ||
      TYPEWEIGHT(rhs) !== TYPEWEIGHT_NUMBER) {
    THROW(INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
  }

  var result = NUMERIC_VALUE(lhs * rhs);
  if (TYPEWEIGHT(result) !== TYPEWEIGHT_NUMBER) {
    THROW(INTERNAL.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic division
///
/// both operands must be numeric or this function will fail
////////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_DIVIDE (lhs, rhs) {
  "use strict";

  if (TYPEWEIGHT(lhs) !== TYPEWEIGHT_NUMBER ||
      TYPEWEIGHT(rhs) !== TYPEWEIGHT_NUMBER) {
    THROW(INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
  }
  
  if (rhs === 0) {
    THROW(INTERNAL.errors.ERROR_QUERY_DIVISION_BY_ZERO);
  }

  var result = NUMERIC_VALUE(lhs / rhs);
  if (TYPEWEIGHT(result) !== TYPEWEIGHT_NUMBER) {
    THROW(INTERNAL.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform artithmetic modulus
///
/// both operands must be numeric or this function will fail
////////////////////////////////////////////////////////////////////////////////

function ARITHMETIC_MODULUS (lhs, rhs) {
  "use strict";

  if (TYPEWEIGHT(lhs) !== TYPEWEIGHT_NUMBER ||
      TYPEWEIGHT(rhs) !== TYPEWEIGHT_NUMBER) {
    THROW(INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
  }

  if (rhs === 0) {
    THROW(INTERNAL.errors.ERROR_QUERY_DIVISION_BY_ZERO);
  }

  var result = NUMERIC_VALUE(lhs % rhs);
  if (TYPEWEIGHT(result) !== TYPEWEIGHT_NUMBER) {
    THROW(INTERNAL.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE);
  }

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  string functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief perform string concatenation
///
/// all operands must be strings or this function will fail
////////////////////////////////////////////////////////////////////////////////

function STRING_CONCAT () {
  "use strict";

  var result = '', i;

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_NULL) {
        ARG_CHECK(element, TYPEWEIGHT_STRING, "CONCAT");
        result += element;
      }
    }
  }

  return result; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform string concatenation using a separator character
///
/// all operands must be strings or this function will fail
////////////////////////////////////////////////////////////////////////////////

function STRING_CONCAT_SEPARATOR () {
  "use strict";

  var separator, found = false, result = '', i;

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (i > 0 && TYPEWEIGHT(element) === TYPEWEIGHT_NULL) {
        continue;
      }

      ARG_CHECK(element, TYPEWEIGHT_STRING, "CONCAT_SEPARATOR");

      if (i === 0 || i === "0") {
        separator = element;
        continue;
      }
      else if (found) {
        result += separator;
      }

      found = true;
      result += element;
    }
  }

  return result; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the length of a string in characters (not bytes)
///
/// the input operand must be a string or this function will fail
////////////////////////////////////////////////////////////////////////////////

function CHAR_LENGTH (value) {
  "use strict";

  ARG_CHECK(value, TYPEWEIGHT_STRING, "CHAR_LENGTH");

  return value.length;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to lower case
///
/// the input operand must be a string or this function will fail
////////////////////////////////////////////////////////////////////////////////

function STRING_LOWER (value) {
  "use strict";

  ARG_CHECK(value, TYPEWEIGHT_STRING, "LOWER");

  return value.toLowerCase();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a string to upper case
///
/// the input operand must be a string or this function will fail
////////////////////////////////////////////////////////////////////////////////

function STRING_UPPER (value) {
  "use strict";

  ARG_CHECK(value, TYPEWEIGHT_STRING, "UPPER");

  return value.toUpperCase();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a substring of the string
///
/// the input operand must be a string or this function will fail
////////////////////////////////////////////////////////////////////////////////

function STRING_SUBSTRING (value, offset, count) {
  "use strict";

  ARG_CHECK(value, TYPEWEIGHT_STRING, "SUBSTRING");
  ARG_CHECK(offset, TYPEWEIGHT_NUMBER, "SUBSTRING");

  if (count !== undefined) {
    ARG_CHECK(count, TYPEWEIGHT_NUMBER, "SUBSTRING");
  }

  return value.substr(offset, count);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief searches a substring in a string
///
/// the two input operands must be strings or this function will fail
////////////////////////////////////////////////////////////////////////////////

function STRING_CONTAINS (value, search, returnIndex) {
  "use strict";

  ARG_CHECK(value, TYPEWEIGHT_STRING, "CONTAINS");
  ARG_CHECK(search, TYPEWEIGHT_STRING, "CONTAINS");

  if (returnIndex !== undefined) {
    ARG_CHECK(returnIndex, TYPEWEIGHT_BOOL, "CONTAINS");
  }
    

  var result;
  if (search.length === 0) {
    result = -1;
  }
  else {
    result = value.indexOf(search);
  }

  if (returnIndex) {
    return result;
  }

  return (result !== -1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief searches a substring in a string, using a regex
///
/// the two input operands must be strings or this function will fail
////////////////////////////////////////////////////////////////////////////////

function STRING_LIKE (value, regex, caseInsensitive) {
  "use strict";

  ARG_CHECK(value, TYPEWEIGHT_STRING, "LIKE");

  var modifiers = '';
  if (caseInsensitive) {
    modifiers += 'i';
  }

  if (RegexCache[modifiers][regex] === undefined) {
    RegexCache[modifiers][regex] = COMPILE_REGEX(regex, modifiers);
  }
  
  try {
    return RegexCache[modifiers][regex].test(value);
  }
  catch (err) {
    THROW(INTERNAL.errors.ERROR_QUERY_INVALID_REGEX, "LIKE");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the leftmost parts of a string
////////////////////////////////////////////////////////////////////////////////

function STRING_LEFT (value, length) {
  "use strict";

  ARG_CHECK(value, TYPEWEIGHT_STRING, "LEFT");
  ARG_CHECK(length, TYPEWEIGHT_NUMBER, "LEFT");

  if (length < 0) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "LEFT");
    ARG_CHECK(length, TYPEWEIGHT_NUMBER, "LEFT");
  }
  length = parseInt(length, 10);
  if (length === 0) {
    return "";
  }

  return value.substr(0, length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the rightmost parts of a string
////////////////////////////////////////////////////////////////////////////////

function STRING_RIGHT (value, length) {
  "use strict";

  ARG_CHECK(value, TYPEWEIGHT_STRING, "RIGHT");
  ARG_CHECK(length, TYPEWEIGHT_NUMBER, "RIGHT");
  
  if (length < 0) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "RIGHT");
    ARG_CHECK(length, TYPEWEIGHT_NUMBER, "RIGHT");
  }
  length = parseInt(length, 10);
  if (length === 0) {
    return "";
  }

  var len = value.length;
  if (length > len) {
    length = len;
  }
  return value.substr(value.length - length, length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the rightmost parts of a string
////////////////////////////////////////////////////////////////////////////////

function STRING_TRIM (value, type) {
  "use strict";

  ARG_CHECK(value, TYPEWEIGHT_STRING, "TRIM");

  if (type !== undefined) {
    ARG_CHECK(type, TYPEWEIGHT_NUMBER, "TRIM");
    type = parseInt(type, 10);
    if (type < 0 || type > 2) {
      THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "TRIM");
    }
  }
  else {
    type = 0;
  }

  var result;
  if (type === 0) {
    // TRIM(, 0)
    result = value.replace(/(^\s+)|\s+$/g, '');
  }
  else if (type === 1) {
    // TRIM(, 1)
    result = value.replace(/^\s+/g, '');
  }
  else if (type === 2) {
    // TRIM(, 2)
    result = value.replace(/\s+$/g, '');
  }

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                typecast functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to a bool
///
/// the operand can have any type, always returns a bool
////////////////////////////////////////////////////////////////////////////////

function CAST_BOOL (value) {
  "use strict";

  switch (TYPEWEIGHT(value)) {
    case TYPEWEIGHT_NULL:
      return false;
    case TYPEWEIGHT_BOOL:
      return value;
    case TYPEWEIGHT_NUMBER:
      return (value !== 0);
    case TYPEWEIGHT_STRING: 
      return (value !== '');
    case TYPEWEIGHT_LIST:
      return (value.length > 0);
    case TYPEWEIGHT_DOCUMENT:
      return (KEYS(value, false).length > 0);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to a number
///
/// the operand can have any type, always returns a number
////////////////////////////////////////////////////////////////////////////////

function CAST_NUMBER (value) {
  "use strict";

  switch (TYPEWEIGHT(value)) {
    case TYPEWEIGHT_NULL:
    case TYPEWEIGHT_LIST:
    case TYPEWEIGHT_DOCUMENT:
      return 0.0;
    case TYPEWEIGHT_BOOL:
      return (value ? 1 : 0);
    case TYPEWEIGHT_NUMBER:
      return value;
    case TYPEWEIGHT_STRING:
      var result = parseFloat(value);
      return ((TYPEWEIGHT(result) === TYPEWEIGHT_NUMBER) ? result : 0);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to a string
///
/// the operand can have any type, always returns a string
////////////////////////////////////////////////////////////////////////////////

function CAST_STRING (value) {
  "use strict";

  switch (TYPEWEIGHT(value)) {
    case TYPEWEIGHT_STRING:
      return value;
    case TYPEWEIGHT_NULL:
      return 'null';
    case TYPEWEIGHT_BOOL:
      return (value ? 'true' : 'false');
    case TYPEWEIGHT_NUMBER:
    case TYPEWEIGHT_LIST:
    case TYPEWEIGHT_DOCUMENT:
      return value.toString();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cast to a list
///
/// the operand can have any type, always returns a list
////////////////////////////////////////////////////////////////////////////////

function CAST_LIST (value) {
  "use strict";

  switch (TYPEWEIGHT(value)) {
    case TYPEWEIGHT_LIST:
      return value;
    case TYPEWEIGHT_NULL:
      return [ ];
    case TYPEWEIGHT_BOOL:
    case TYPEWEIGHT_NUMBER:
    case TYPEWEIGHT_STRING:
      return [ value ];
    case TYPEWEIGHT_DOCUMENT:
      return VALUES(value);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                               typecheck functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type null
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function IS_NULL (value) {
  "use strict";

  return (TYPEWEIGHT(value) === TYPEWEIGHT_NULL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type bool
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function IS_BOOL (value) {
  "use strict";

  return (TYPEWEIGHT(value) === TYPEWEIGHT_BOOL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type number
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function IS_NUMBER (value) {
  "use strict";

  return (TYPEWEIGHT(value) === TYPEWEIGHT_NUMBER);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type string
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function IS_STRING (value) {
  "use strict";

  return (TYPEWEIGHT(value) === TYPEWEIGHT_STRING);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type list
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function IS_LIST (value) {
  "use strict";

  return (TYPEWEIGHT(value) === TYPEWEIGHT_LIST);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if value is of type document
///
/// returns a bool
////////////////////////////////////////////////////////////////////////////////

function IS_DOCUMENT (value) {
  "use strict";

  return (TYPEWEIGHT(value) === TYPEWEIGHT_DOCUMENT);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 numeric functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief integer closest to value, not greater than value
////////////////////////////////////////////////////////////////////////////////

function NUMBER_FLOOR (value) {
  "use strict";

  if (! IS_NUMBER(value)) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "FLOOR");
  }
  
  return NUMERIC_VALUE(Math.floor(value));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief integer closest to value and not less than value
////////////////////////////////////////////////////////////////////////////////

function NUMBER_CEIL (value) {
  "use strict";

  if (! IS_NUMBER(value)) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "CEIL");
  }
  
  return NUMERIC_VALUE(Math.ceil(value));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief integer closest to value 
////////////////////////////////////////////////////////////////////////////////

function NUMBER_ROUND (value) {
  "use strict";

  if (! IS_NUMBER(value)) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "ROUND");
  }
  
  return NUMERIC_VALUE(Math.round(value));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief absolute value
////////////////////////////////////////////////////////////////////////////////

function NUMBER_ABS (value) {
  "use strict";

  if (! IS_NUMBER(value)) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "ABS");
  }
  
  return NUMERIC_VALUE(Math.abs(value));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a random value between 0 and 1
////////////////////////////////////////////////////////////////////////////////

function NUMBER_RAND () {
  "use strict";

  return Math.random();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief square root
////////////////////////////////////////////////////////////////////////////////

function NUMBER_SQRT (value) {
  "use strict";

  if (! IS_NUMBER(value)) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "SQRT");
  }
  
  return NUMERIC_VALUE(Math.sqrt(value));
}

// -----------------------------------------------------------------------------
// --SECTION--                                        high level query functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sort the results
////////////////////////////////////////////////////////////////////////////////

function SORT (value, sortFunction) {
  "use strict";

  LIST(value);
 
  var n = value.length;
  if (n > 0) {
    value.sort(sortFunction);
  }

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief group the results
////////////////////////////////////////////////////////////////////////////////

function GROUP (value, sortFunction, groupFunction, into) {
  "use strict";

  LIST(value);

  var n = value.length;
  if (n === 0) {
    return [ ];
  }

  var augmented = [ ], i;
  for (i = 0; i < n; ++i) {
    augmented.push([ i, value[i] ]);
  }

  SORT(augmented, sortFunction);

  var result = [ ], currentGroup, oldGroup;
  
  for (i = 0; i < n; ++i) {
    var row = augmented[i][1];
    var groupValue = groupFunction(row);

    if (RELATIONAL_UNEQUAL(oldGroup, groupValue)) {
      oldGroup = CLONE(groupValue);

      if (currentGroup) {
        result.push(CLONE(currentGroup));
      }
      
      currentGroup = groupValue;
      if (into) {
        currentGroup[into] = [ ];
      }
    }

    if (into) {
      currentGroup[into].push(CLONE(row));
    }
  }

  if (currentGroup) {
    result.push(CLONE(currentGroup));
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief limit the number of results
////////////////////////////////////////////////////////////////////////////////

function LIMIT (value, offset, count) {
  "use strict";

  LIST(value);

  // check value type for offset and count parameters
  if (TYPEWEIGHT(offset) !== TYPEWEIGHT_NUMBER ||
      TYPEWEIGHT(count) !== TYPEWEIGHT_NUMBER) {
    THROW(INTERNAL.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE);
  }

  if (count < 0) {
    THROW(INTERNAL.errors.ERROR_QUERY_NUMBER_OUT_OF_RANGE);
  }

  return value.slice(offset, offset + count);
}

// -----------------------------------------------------------------------------
// --SECTION--                                         list processing functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the length of a list, document or string
////////////////////////////////////////////////////////////////////////////////

function LENGTH (value) {
  "use strict";

  var result, typeWeight = TYPEWEIGHT(value);
  
  if (typeWeight === TYPEWEIGHT_LIST || typeWeight === TYPEWEIGHT_STRING) {
    result = value.length;
  }
  else if (typeWeight === TYPEWEIGHT_DOCUMENT) {
    result = KEYS(value, false).length;
  }
  else {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "LENGTH");
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the first element of a list
////////////////////////////////////////////////////////////////////////////////

function FIRST (value) {
  "use strict";

  LIST(value);

  if (value.length === 0) {
    return null;
  }

  return value[0];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last element of a list
////////////////////////////////////////////////////////////////////////////////

function LAST (value) {
  "use strict";

  LIST(value);

  if (value.length === 0) {
    return null;
  }

  return value[value.length - 1];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the position of an element in a list
////////////////////////////////////////////////////////////////////////////////

function POSITION (value, search, returnIndex) {
  "use strict";

  LIST(value);
  returnIndex = returnIndex || false;

  var i, n = value.length;

  if (n > 0) {
    for (i = 0; i < n; ++i) {
      if (RELATIONAL_EQUAL(value[i], search)) {
        return returnIndex ? i : true;
      }
    }
  }

  return returnIndex ? -1 : false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the nth element in a list, or null if the item does not exist
////////////////////////////////////////////////////////////////////////////////

function NTH (value, position) {
  "use strict";

  LIST(value);
  if (position < 0 || position >= value.length) {
    return null;
  }

  return value[position];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reverse the elements in a list or in a string
////////////////////////////////////////////////////////////////////////////////

function REVERSE (value) {
  "use strict";

  if (TYPEWEIGHT(value) === TYPEWEIGHT_STRING) {
    return value.split("").reverse().join(""); 
  }

  LIST(value);

  return value.reverse();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a range of values
////////////////////////////////////////////////////////////////////////////////

function RANGE (from, to, step) {
  "use strict";

  if (step === undefined) {
    if (from <= to) {
      step = 1;
    }
    else {
      step = -1;
    }
  }
 
  // check from, to and step values 
  ARG_CHECK(from, TYPEWEIGHT_NUMBER, "RANGE");
  ARG_CHECK(to, TYPEWEIGHT_NUMBER, "RANGE");
  ARG_CHECK(step, TYPEWEIGHT_NUMBER, "RANGE");

  // check if we would run into an endless loop
  if (step === 0) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "RANGE");
  }
  if (from < to && step < 0) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "RANGE");
  }
  if (from > to && step > 0) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "RANGE");
  }

  var result = [ ], i;
  if (step < 0 && to <= from) {
    for (i = from; i >= to; i += step) {
      result.push(i);
    }
  }
  else {
    for (i = from; i <= to; i += step) {
      result.push(i);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a list of unique elements from the list
////////////////////////////////////////////////////////////////////////////////

function UNIQUE (values) {
  "use strict";

  LIST(values);

  var keys = { }, result = [ ], a;

  values.forEach(function (value) {
    var normalized = NORMALIZE(value);
    var key = JSON.stringify(normalized);

    if (! keys.hasOwnProperty(key)) {
      keys[key] = normalized;
    }
  });

  for (a in keys) {
    if (keys.hasOwnProperty(a)) {
      result.push(keys[a]);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the union (all) of all arguments
////////////////////////////////////////////////////////////////////////////////

function UNION () {
  "use strict";

  var result = [ ], i;

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_LIST) {
        THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "UNION");
      }

      var n = element.length, j;

      for (j = 0; j < n; ++j) {
        result.push(element[j]);
      }
    }
  }

  return result; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the union (distinct) of all arguments
////////////////////////////////////////////////////////////////////////////////

function UNION_DISTINCT () {
  "use strict";

  var keys = { }, i;

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_LIST) {
        THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "UNION_DISTINCT");
      }
      
      var n = element.length, j;

      for (j = 0; j < n; ++j) {
        var normalized = NORMALIZE(element[j]);
        var key = JSON.stringify(normalized);

        if (! keys.hasOwnProperty(key)) {
          keys[key] = normalized;
        }
      }
    }
  }

  var result = [ ];
  for (i in keys) {
    if (keys.hasOwnProperty(i)) {
      result.push(keys[i]);
    }
  }

  return result; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief subtract lists from other lists
////////////////////////////////////////////////////////////////////////////////

function MINUS () {
  "use strict";

  var keys = { }, i, first = true;

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_LIST) {
        THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "MINUS");
      }
      
      var n = element.length, j;

      for (j = 0; j < n; ++j) {
        var normalized = NORMALIZE(element[j]);
        var key = JSON.stringify(normalized);
        var contained = keys.hasOwnProperty(key);

        if (first) {
          if (! contained) {
            keys[key] = normalized;
          }
        }
        else if (contained) {
          delete keys[key];
        }
      }

      first = false;
    }
  }

  var result = [ ];
  for (i in keys) {
    if (keys.hasOwnProperty(i)) {
      result.push(keys[i]);
    }
  }

  return result; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the intersection of all arguments
////////////////////////////////////////////////////////////////////////////////

function INTERSECTION () {
  "use strict";

  var result = [ ], i, first = true, keys = { };
        
  var func = function (value) {
    var normalized = NORMALIZE(value);
    keys[JSON.stringify(normalized)] = normalized;
  };

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_LIST) {
        THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "INTERSECTION");
      }
 
      if (first) {
        element.forEach(func);
        first = false;
      }
      else {
        var j, newKeys = { };
        for (j = 0; j < element.length; ++j) {
          var normalized = NORMALIZE(element[j]);
          var key = JSON.stringify(normalized);

          if (keys.hasOwnProperty(key)) {
            newKeys[key] = normalized;
          }
        }
        keys = newKeys;
        newKeys = null;
      }

    }
  }

  for (i in keys) {
    if (keys.hasOwnProperty(i)) {
      result.push(keys[i]);
    }
  }

  return result; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum of all values
////////////////////////////////////////////////////////////////////////////////

function MAX (values) {
  "use strict";

  LIST(values);

  var value, result = null;
  var i, n;
  
  for (i = 0, n = values.length; i < n; ++i) {
    value = values[i];
    if (TYPEWEIGHT(value) !== TYPEWEIGHT_NULL) { 
      if (result === null || RELATIONAL_GREATER(value, result)) {
        result = value;
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief minimum of all values
////////////////////////////////////////////////////////////////////////////////

function MIN (values) {
  "use strict";

  LIST(values);

  var value, result = null;
  var i, n;
  
  for (i = 0, n = values.length; i < n; ++i) {
    value = values[i];
    if (TYPEWEIGHT(value) !== TYPEWEIGHT_NULL) { 
      if (result === null || RELATIONAL_LESS(value, result)) {
        result = value;
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sum of all values
////////////////////////////////////////////////////////////////////////////////

function SUM (values) {
  "use strict";

  LIST(values);

  var i, n;
  var value, typeWeight, result = 0;

  for (i = 0, n = values.length; i < n; ++i) {
    value = values[i];
    typeWeight = TYPEWEIGHT(value);

    if (typeWeight !== TYPEWEIGHT_NULL) {
      if (typeWeight !== TYPEWEIGHT_NUMBER) {
        THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "SUM");
      }

      result += value;
    }
  }

  return NUMERIC_VALUE(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief average of all values
////////////////////////////////////////////////////////////////////////////////

function AVERAGE (values) {
  "use strict";

  LIST(values);

  var current, typeWeight, sum = 0;
  var i, j, n;

  for (i = 0, j = 0, n = values.length; i < n; ++i) {
    current = values[i];
    typeWeight = TYPEWEIGHT(current);

    if (typeWeight !== TYPEWEIGHT_NULL) {
      if (typeWeight !== TYPEWEIGHT_NUMBER) {
        THROW(INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
      }

      sum += current;
      j++;
    }
  }

  if (j === 0) {
    return null;
  }

  return NUMERIC_VALUE(sum / j);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief median of all values
////////////////////////////////////////////////////////////////////////////////

function MEDIAN (values) {
  "use strict";

  LIST(values);
  
  var copy = [ ], current, typeWeight;
  var i, n;

  for (i = 0, n = values.length; i < n; ++i) {
    current = values[i];
    typeWeight = TYPEWEIGHT(current);

    if (typeWeight !== TYPEWEIGHT_NULL) {
      if (typeWeight !== TYPEWEIGHT_NUMBER) {
        THROW(INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
      }

      copy.push(current);
    }
  }

  if (copy.length === 0) {
    return null;
  }

  copy.sort(RELATIONAL_CMP);

  var midpoint = copy.length / 2;
  if (copy.length % 2 === 0) {
    return NUMERIC_VALUE((copy[midpoint - 1] + copy[midpoint]) / 2);
  }

  return NUMERIC_VALUE(copy[Math.floor(midpoint)]);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief variance of all values
////////////////////////////////////////////////////////////////////////////////

function VARIANCE (values) {
  "use strict";

  LIST(values);

  var mean = 0, m2 = 0, current, typeWeight, delta;
  var i, j, n;

  for (i = 0, j = 0, n = values.length; i < n; ++i) {
    current = values[i];
    typeWeight = TYPEWEIGHT(current);

    if (typeWeight !== TYPEWEIGHT_NULL) {
      if (typeWeight !== TYPEWEIGHT_NUMBER) {
        THROW(INTERNAL.errors.ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
      }

      delta = current - mean;
      mean += delta / ++j;
      m2 += delta * (current - mean);
    }
  }

  return {
    n: j,
    value: m2
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sample variance of all values
////////////////////////////////////////////////////////////////////////////////

function VARIANCE_SAMPLE (values) {
  "use strict";

  var result = VARIANCE(values);

  if (result.n < 2) {
    return null;
  }

  return NUMERIC_VALUE(result.value / (result.n - 1));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief population variance of all values
////////////////////////////////////////////////////////////////////////////////

function VARIANCE_POPULATION (values) {
  "use strict";

  var result = VARIANCE(values);

  if (result.n < 1) {
    return null;
  }

  return NUMERIC_VALUE(result.value / result.n);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief standard deviation of all values
////////////////////////////////////////////////////////////////////////////////

function STDDEV_SAMPLE (values) {
  "use strict";

  var result = VARIANCE(values);

  if (result.n < 2) {
    return null;
  }

  return NUMERIC_VALUE(Math.sqrt(result.value / (result.n - 1)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief standard deviation of all values
////////////////////////////////////////////////////////////////////////////////

function STDDEV_POPULATION (values) {
  "use strict";

  var result = VARIANCE(values);

  if (result.n < 1) {
    return null;
  }

  return NUMERIC_VALUE(Math.sqrt(result.value / result.n));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     geo functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return at most <limit> documents near a certain point
////////////////////////////////////////////////////////////////////////////////

function GEO_NEAR (collection, latitude, longitude, limit, distanceAttribute) {
  "use strict";

  var idx = INDEX(COLLECTION(collection), [ "geo1", "geo2" ]); 
  if (idx === null) {
    THROW(INTERNAL.errors.ERROR_QUERY_GEO_INDEX_MISSING, collection);
  }

  if (limit === null || limit === undefined) {
    // use default value
    limit = 100;
  }

  var result = COLLECTION(collection).NEAR(idx, latitude, longitude, limit);
  if (distanceAttribute === null || distanceAttribute === undefined) {
    return result.documents;
  }

  // inject distances
  var documents = result.documents;
  var distances = result.distances;
  var n = documents.length, i;
  for (i = 0; i < n; ++i) {
    documents[i][distanceAttribute] = distances[i];
  }

  return documents;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return documents within <radius> around a certain point
////////////////////////////////////////////////////////////////////////////////

function GEO_WITHIN (collection, latitude, longitude, radius, distanceAttribute) {
  "use strict";

  var idx = INDEX(COLLECTION(collection), [ "geo1", "geo2" ]); 
  if (idx === null) {
    THROW(INTERNAL.errors.ERROR_QUERY_GEO_INDEX_MISSING, collection);
  }

  var result = COLLECTION(collection).WITHIN(idx, latitude, longitude, radius);
  if (distanceAttribute === null || distanceAttribute === undefined) {
    return result.documents;
  }

  // inject distances
  var documents = result.documents;
  var distances = result.distances;
  var n = documents.length, i;
  for (i = 0; i < n; ++i) {
    documents[i][distanceAttribute] = distances[i];
  }

  return documents;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                fulltext functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return documents that match a fulltext query
////////////////////////////////////////////////////////////////////////////////

function FULLTEXT (collection, attribute, query) {
  "use strict";

  var idx = INDEX_FULLTEXT(COLLECTION(collection), attribute);
  if (idx === null) {
    THROW(INTERNAL.errors.ERROR_QUERY_FULLTEXT_INDEX_MISSING, collection);
  }

  var result = COLLECTION(collection).FULLTEXT(idx, query);
  return result.documents;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    misc functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the first alternative that's not null until there are no more 
/// alternatives. if neither of the alternatives is a value other than null, 
/// then null will be returned
///
/// the operands can have any type
////////////////////////////////////////////////////////////////////////////////

function NOT_NULL () {
  "use strict";

  var i;
  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];
  
      if (TYPEWEIGHT(element) !== TYPEWEIGHT_NULL) {
        return element;
      }
    }
  }

  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the first alternative that's a list until there are no more
/// alternatives. if neither of the alternatives is a list, then null will be
/// returned
///
/// the operands can have any type
////////////////////////////////////////////////////////////////////////////////

function FIRST_LIST () {
  "use strict";

  var i;
  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) === TYPEWEIGHT_LIST) {
        return element;
      }
    }
  }

  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the first alternative that's a document until there are no 
/// more alternatives. if neither of the alternatives is a document, then null 
/// will be returned
///
/// the operands can have any type
////////////////////////////////////////////////////////////////////////////////

function FIRST_DOCUMENT () {
  "use strict";

  var i;
  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];
  
      if (TYPEWEIGHT(element) === TYPEWEIGHT_DOCUMENT) {
        return element;
      }
    }
  }

  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the parts of a document identifier separately
///
/// returns a document with the attributes `collection` and `key` or fails if
/// the individual parts cannot be determined.
////////////////////////////////////////////////////////////////////////////////

function PARSE_IDENTIFIER (value) {
  "use strict";

  if (TYPEWEIGHT(value) === TYPEWEIGHT_STRING) {
    var parts = value.split('/');
    if (parts.length === 2) {
      return { 
        collection: parts[0],
        key: parts[1]
      };
    }
    // fall through intentional
  }
  else if (TYPEWEIGHT(value) === TYPEWEIGHT_DOCUMENT) {
    if (value.hasOwnProperty('_id')) {
      return PARSE_IDENTIFIER(value._id);
    }
    // fall through intentional
  }
  
  THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "PARSE_IDENTIFIER");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a document has a specific attribute
////////////////////////////////////////////////////////////////////////////////

function HAS (element, name) {
  "use strict";

  if (TYPEWEIGHT(element) === TYPEWEIGHT_NULL) {
    return false;
  }

  if (TYPEWEIGHT(element) !== TYPEWEIGHT_DOCUMENT) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "HAS");
  }

  return element.hasOwnProperty(name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the attribute names of a document as a list
////////////////////////////////////////////////////////////////////////////////

function ATTRIBUTES (element, removeInternal, sort) {
  "use strict";

  if (TYPEWEIGHT(element) !== TYPEWEIGHT_DOCUMENT) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "ATTRIBUTES");
  }

  if (removeInternal) {  
    var a, result = [ ];

    for (a in element) {
      if (element.hasOwnProperty(a)) {
        if (a.substring(0, 1) !== '_') {
          result.push(a);
        }
      }
    }
  
    if (sort) {
      result.sort();
    }

    return result;
  }
  
  return KEYS(element, sort);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unset specific attributes from a document
////////////////////////////////////////////////////////////////////////////////

function UNSET (value) {
  "use strict";

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_DOCUMENT) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "UNSET");
  }

  var result = { }, keys = EXTRACT_KEYS(arguments, 1, "UNSET"), i;
  // copy over all that is left
  for (i in value) {
    if (value.hasOwnProperty(i)) {
      if (keys[i] !== true) {
        result[i] = CLONE(value[i]);
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief keep specific attributes from a document
////////////////////////////////////////////////////////////////////////////////

function KEEP (value) {
  "use strict";

  if (TYPEWEIGHT(value) !== TYPEWEIGHT_DOCUMENT) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "KEEP");
  }

  var keys = EXTRACT_KEYS(arguments, 1, "KEEP"), i;

  // copy over all that is left
  var result = { };
  for (i in keys) {
    if (keys.hasOwnProperty(i)) {
      if (value.hasOwnProperty(i)) {
        result[i] = CLONE(value[i]);
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge all arguments
////////////////////////////////////////////////////////////////////////////////

function MERGE () {
  "use strict";

  var result = { }, i, a;

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_DOCUMENT) {
        THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "MERGE");
      }

      for (a in element) {
        if (element.hasOwnProperty(a)) {
          result[a] = element[a];
        }
      }
    }
  }

  return result; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge all arguments recursively
////////////////////////////////////////////////////////////////////////////////

function MERGE_RECURSIVE () {
  "use strict";

  var result = { }, i, recurse;
      
  recurse = function (old, element) {
    var r = CLONE(old), a;

    for (a in element) {
      if (element.hasOwnProperty(a)) {
        if (r.hasOwnProperty(a) && TYPEWEIGHT(element[a]) === TYPEWEIGHT_DOCUMENT) {
          r[a] = recurse(r[a], element[a]);
        }
        else {
          r[a] = element[a];
        }
      }
    }
    return r;
  };

  for (i in arguments) {
    if (arguments.hasOwnProperty(i)) {
      var element = arguments[i];

      if (TYPEWEIGHT(element) !== TYPEWEIGHT_DOCUMENT) {
        THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "MERGE_RECURSIVE");
      }

      result = recurse(result, element);
    }
  }

  return result; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare an object against a list of examples and return whether the
/// object matches any of the examples. returns the example index or a bool,
/// depending on the value of the control flag (3rd) parameter
////////////////////////////////////////////////////////////////////////////////

function MATCHES (element, examples, returnIndex) {
  "use strict";

  if (TYPEWEIGHT(element) !== TYPEWEIGHT_DOCUMENT) {
    return false;
  }

  if (! Array.isArray(examples)) {
    examples = [ examples ];
  }

  if (examples.length === 0) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "MATCHES");
  }

  returnIndex = returnIndex || false;
  var i;

  for (i = 0; i < examples.length; ++i) {
    var example = examples[i];
    var result = true;

    if (TYPEWEIGHT(example) !== TYPEWEIGHT_DOCUMENT) {
      THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "MATCHES");
    }

    var keys = KEYS(example), key, j;
    for (j = 0; j < keys.length; ++j) {
      key = keys[j];

      if (! RELATIONAL_EQUAL(element[key], example[key])) {
        result = false;
        break;
      }
    }

    if (result) {
      // 3rd parameter determines whether we return the index or a bool flag
      return (returnIndex ? i : true);
    }
  }

  return (returnIndex ? -1 : false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief passthru the argument
///
/// this function is marked as non-deterministic so its argument withstands
/// query optimisation. this function can be used for testing
////////////////////////////////////////////////////////////////////////////////

function PASSTHRU (value) {
  "use strict";

  return value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sleep
///
/// sleep for the specified duration
////////////////////////////////////////////////////////////////////////////////

function SLEEP (duration) {
  "use strict";

  if (TYPEWEIGHT(duration) !== TYPEWEIGHT_NUMBER) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "SLEEP");
  }

  INTERNAL.wait(duration);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief always fail
///
/// this function is non-deterministic so it is not executed at query 
/// optimisation time. this function can be used for testing
////////////////////////////////////////////////////////////////////////////////

function FAIL (message) {
  "use strict";

  if (TYPEWEIGHT(message) === TYPEWEIGHT_STRING) {
    THROW(INTERNAL.errors.ERROR_QUERY_FAIL_CALLED, message);
  }

  THROW(INTERNAL.errors.ERROR_QUERY_FAIL_CALLED, "");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   graph functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief find all paths through a graph, INTERNAL part called recursively
////////////////////////////////////////////////////////////////////////////////

function GRAPH_SUBNODES (searchAttributes, vertexId, visited, edges, vertices, level) {
  "use strict";

  var result = [ ];

  if (level >= searchAttributes.minLength) {
    result.push({ 
      vertices : vertices, 
      edges : edges,
      source : vertices[0],
      destination : vertices[vertices.length - 1]
    });
  }

  if (level + 1 > searchAttributes.maxLength) {
    return result;
  }
        
        
  var subEdges;

  if (searchAttributes.direction === 1) {
    subEdges = searchAttributes.edgeCollection.outEdges(vertexId);
  }
  else if (searchAttributes.direction === 2) {
    subEdges = searchAttributes.edgeCollection.inEdges(vertexId);
  }
  else if (searchAttributes.direction === 3) {
    subEdges = searchAttributes.edgeCollection.edges(vertexId);
  }

  var i, j, k;
  for (i = 0; i < subEdges.length; ++i) {
    var subEdge = subEdges[i];
    var targets = [ ];

    if ((searchAttributes.direction === 1 || searchAttributes.direction === 3) && 
        (subEdge._to !== vertexId)) {
      targets.push(subEdge._to);
    }
    if ((searchAttributes.direction === 2 || searchAttributes.direction === 3) && 
        (subEdge._from !== vertexId)) {
      targets.push(subEdge._from);
    }

    for (j = 0; j < targets.length; ++j) {
      var targetId = targets[j];
      
      if (! searchAttributes.followCycles) {
        if (visited[targetId]) {
          continue;
        }
        visited[targetId] = true;
      }

      var clonedEdges = CLONE(edges);
      var clonedVertices = CLONE(vertices);
      try { 
        clonedVertices.push(INTERNAL.db._document(targetId));
        clonedEdges.push(subEdge);
      }
      catch (e) {
        // avoid "document not found error" in case referenced vertices were deleted
      }
      
      var connected = GRAPH_SUBNODES(searchAttributes, 
                                     targetId, 
                                     CLONE(visited), 
                                     clonedEdges, 
                                     clonedVertices, 
                                     level + 1);
      for (k = 0; k < connected.length; ++k) {
        result.push(connected[k]);
      }

      if (! searchAttributes.followCycles) {
        delete visited[targetId];
      }
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find all paths through a graph
////////////////////////////////////////////////////////////////////////////////

function GRAPH_PATHS (vertices, edgeCollection, direction, followCycles, minLength, maxLength) {
  "use strict";

  var searchDirection;
  
  direction      = direction || "outbound";
  followCycles   = followCycles || false;
  minLength      = minLength || 0;
  maxLength      = maxLength !== undefined ? maxLength : 10;
  
  LIST(vertices);

  // validate arguments
  if (direction === "outbound") {
    searchDirection = 1;
  }
  else if (direction === "inbound") {
    searchDirection = 2;
  }
  else if (direction === "any") {
    searchDirection = 3;
  }
  else {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "PATHS");
  }

  if (minLength < 0 || maxLength < 0 || minLength > maxLength) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "PATHS");
  }

  var searchAttributes = { 
    edgeCollection : COLLECTION(edgeCollection),
    minLength : minLength, 
    maxLength : maxLength, 
    direction : searchDirection,
    followCycles : followCycles
  };

  var result = [ ];
  var n = vertices.length, i, j;
  for (i = 0; i < n; ++i) {
    var vertex = vertices[i];
    var visited = { };

    visited[vertex._id] = true;
    var connected = GRAPH_SUBNODES(searchAttributes, vertex._id, visited, [ ], [ vertex ], 0);
    for (j = 0; j < connected.length; ++j) {
      result.push(connected[j]);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief visitor callback function for traversal
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_VISITOR (config, result, vertex, path) {
  "use strict";

  if (config.trackPaths) {
    result.push(CLONE({ vertex: vertex, path: path }));
  }
  else {
    result.push(CLONE({ vertex: vertex }));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief visitor callback function for tree traversal
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_TREE_VISITOR (config, result, vertex, path) {
  "use strict";

  if (result.length === 0) {
    result.push({ }); 
  }
  
  var current = result[0], connector = config.connect, i;

  for (i = 0; i < path.vertices.length; ++i) {
    var v = path.vertices[i];
    if (typeof current[connector] === "undefined") {
      current[connector] = [ ];
    }
    var found = false, j;
    for (j = 0; j < current[connector].length; ++j) {
      if (current[connector][j]._id === v._id) {
        current = current[connector][j];
        found = true;
        break;
      }
    }
    if (! found) {
      current[connector].push(CLONE(v));
      current = current[connector][current[connector].length - 1];
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief expander callback function for traversal
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_FILTER (config, vertex, edge, path) {
  "use strict";

  return MATCHES(edge, config.expandEdgeExamples);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief vertex filter callback function for traversal
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_VERTEX_FILTER (config, vertex, path) {
  "use strict";
  
  if (!MATCHES(vertex, config.filterVertexExamples)) {
    return config.vertexFilterMethod;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check typeweights of params.followEdges/params.filterVertices
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_CHECK_EXAMPLES_TYPEWEIGHTS (examples) {
  "use strict";
  
  if (TYPEWEIGHT(examples) !== TYPEWEIGHT_LIST) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, func);
  }
  if (examples.length === 0) {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, func);
  }
  examples.forEach(function (example) {
    if (TYPEWEIGHT(example) !== TYPEWEIGHT_DOCUMENT) {
      THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, func);
    }
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief traverse a graph
////////////////////////////////////////////////////////////////////////////////

function TRAVERSAL_FUNC (func, vertexCollection, edgeCollection, startVertex, direction, params) {
  "use strict";

  if (startVertex.indexOf('/') === -1) {
    startVertex = vertexCollection + '/' + startVertex;
  }

  vertexCollection = COLLECTION(vertexCollection);
  edgeCollection   = COLLECTION(edgeCollection);
  
  // check followEdges property
  if (params.followEdges) {
    TRAVERSAL_CHECK_EXAMPLES_TYPEWEIGHTS(params.followEdges);
  }
  // check filterVertices property
  if (params.filterVertices) {
    TRAVERSAL_CHECK_EXAMPLES_TYPEWEIGHTS(params.filterVertices);
  }

  if (typeof params.visitor !== "function") {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, func);
  }

  if (params.maxDepth === undefined) {
    // we need to have at least SOME limit to prevent endless iteration
    params.maxDepth = 256;
  }

  var config = {
    connect: params.connect,
    datasource: TRAVERSAL.collectionDatasourceFactory(edgeCollection),
    trackPaths: params.paths || false,
    visitor: params.visitor,
    maxDepth: params.maxDepth,
    minDepth: params.minDepth,
    maxIterations: params.maxIterations,
    uniqueness: params.uniqueness,
    expander: direction
  };

  if (params.followEdges) {
    config.expandFilter = TRAVERSAL_FILTER;
    config.expandEdgeExamples = params.followEdges;
  }

  if (params.filterVertices) {
    config.filter = TRAVERSAL_VERTEX_FILTER;
    config.filterVertexExamples = params.filterVertices;
    config.vertexFilterMethod = params.vertexFilterMethod || ["prune","exclude"];
  }

  if (params._sort) {
    config.sort = function (l, r) { return l._key < r._key ? -1 : 1; };
  }

  var v = null;
  var result = [ ];
  try {
    v = INTERNAL.db._document(startVertex);
  }
  catch (err) {
  }

  if (v !== null) {
    var traverser = new TRAVERSAL.Traverser(config);
    traverser.traverse(result, v);
  }

  return result;
}
 
////////////////////////////////////////////////////////////////////////////////
/// @brief traverse a graph
////////////////////////////////////////////////////////////////////////////////

function GRAPH_TRAVERSAL (vertexCollection, 
                          edgeCollection, 
                          startVertex, 
                          direction, 
                          params) {
  "use strict";

  params.visitor  = TRAVERSAL_VISITOR;

  return TRAVERSAL_FUNC("TRAVERSAL", 
                        vertexCollection, 
                        edgeCollection, 
                        startVertex, 
                        direction, 
                        params);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief traverse a graph and create a hierarchical result
/// this function uses the same setup as the TRAVERSE() function but will use
/// a different visitor to create the result
////////////////////////////////////////////////////////////////////////////////

function GRAPH_TRAVERSAL_TREE (vertexCollection, 
                               edgeCollection, 
                               startVertex, 
                               direction, 
                               connectName, 
                               params) {
  "use strict";

  if (connectName === "") {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "TRAVERSAL_TREE");
  }

  params.strategy = "depthfirst";
  params.order    = "preorder";
  params.visitor  = TRAVERSAL_TREE_VISITOR;
  params.connect  = connectName;

  var result = TRAVERSAL_FUNC("TRAVERSAL_TREE", 
                              vertexCollection, 
                              edgeCollection, 
                              startVertex, 
                              direction, 
                              params);

  if (result.length === 0) {
    return [ ];
  }
  return [ result[0][params.connect] ];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return connected edges
////////////////////////////////////////////////////////////////////////////////

function GRAPH_EDGES (edgeCollection, 
                      vertex, 
                      direction, 
                      examples) {
  "use strict";

  var c = COLLECTION(edgeCollection), result;

  // validate arguments
  if (direction === "outbound") {
    result = c.outEdges(vertex);
  }
  else if (direction === "inbound") {
    result = c.inEdges(vertex);
  }
  else if (direction === "any") {
    result = c.edges(vertex);
  }
  else {
    THROW(INTERNAL.errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "EDGES");
  }

  return FILTER(result, examples);
} 

////////////////////////////////////////////////////////////////////////////////
/// @brief return connected neighbors
////////////////////////////////////////////////////////////////////////////////

function GRAPH_NEIGHBORS (vertexCollection,
                          edgeCollection, 
                          vertex, 
                          direction,
                          examples) {
  "use strict";

  var c = COLLECTION(vertexCollection);

  if (vertex.indexOf('/') === -1) {
    vertex = vertexCollection + '/' + vertex;
  }

  var edges = GRAPH_EDGES(edgeCollection, vertex, direction);
  var result = [ ];
  
  FILTER(edges, examples).forEach (function (e) {
    var key;

    if (direction === "inbound") {
      key = e._from;
    }
    else if (direction === "outbound") {
      key = e._to;
    }
    else if (direction === "any") {
      key = e._from;
      if (key === vertex) {
        key = e._to;
      }
    }

    if (key === vertex) {
      // do not return the start vertex itself
      return;
    }

    try {
      result.push({ edge: CLONE(e), vertex: c.document(key) });
    }
    catch (err) {
    }
  });

  return result;
} 

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

exports.FCALL = FCALL;
exports.FCALL_USER = FCALL_USER;
exports.KEYS = KEYS;
exports.GET_INDEX = GET_INDEX;
exports.DOCUMENT_MEMBER = DOCUMENT_MEMBER;
exports.LIST = LIST;
exports.DOCUMENT = DOCUMENT;
exports.GET_DOCUMENTS = GET_DOCUMENTS;
exports.GET_DOCUMENTS_INCREMENTAL_INIT = GET_DOCUMENTS_INCREMENTAL_INIT;
exports.GET_DOCUMENTS_INCREMENTAL_CONT = GET_DOCUMENTS_INCREMENTAL_CONT;
exports.GET_DOCUMENTS_PRIMARY = GET_DOCUMENTS_PRIMARY;
exports.GET_DOCUMENTS_PRIMARY_LIST = GET_DOCUMENTS_PRIMARY_LIST;
exports.GET_DOCUMENTS_HASH = GET_DOCUMENTS_HASH;
exports.GET_DOCUMENTS_HASH_LIST = GET_DOCUMENTS_HASH_LIST;
exports.GET_DOCUMENTS_EDGE = GET_DOCUMENTS_EDGE;
exports.GET_DOCUMENTS_EDGE_LIST = GET_DOCUMENTS_EDGE_LIST;
exports.GET_DOCUMENTS_BITARRAY = GET_DOCUMENTS_BITARRAY;
exports.GET_DOCUMENTS_BITARRAY_LIST = GET_DOCUMENTS_BITARRAY_LIST;
exports.GET_DOCUMENTS_SKIPLIST = GET_DOCUMENTS_SKIPLIST;
exports.GET_DOCUMENTS_SKIPLIST_LIST = GET_DOCUMENTS_SKIPLIST_LIST;
exports.COLLECTIONS = COLLECTIONS;
exports.TERNARY_OPERATOR = TERNARY_OPERATOR;
exports.TERNARY_OPERATOR_FN = TERNARY_OPERATOR_FN;
exports.LOGICAL_AND = LOGICAL_AND;
exports.LOGICAL_OR = LOGICAL_OR;
exports.LOGICAL_AND_FN = LOGICAL_AND_FN;
exports.LOGICAL_OR_FN = LOGICAL_OR_FN;
exports.LOGICAL_NOT = LOGICAL_NOT;
exports.RELATIONAL_EQUAL = RELATIONAL_EQUAL;
exports.RELATIONAL_UNEQUAL = RELATIONAL_UNEQUAL;
exports.RELATIONAL_GREATER = RELATIONAL_GREATER;
exports.RELATIONAL_GREATEREQUAL = RELATIONAL_GREATEREQUAL;
exports.RELATIONAL_LESS = RELATIONAL_LESS;
exports.RELATIONAL_LESSEQUAL = RELATIONAL_LESSEQUAL;
exports.RELATIONAL_CMP = RELATIONAL_CMP;
exports.RELATIONAL_IN = RELATIONAL_IN;
exports.UNARY_PLUS = UNARY_PLUS;
exports.UNARY_MINUS = UNARY_MINUS;
exports.ARITHMETIC_PLUS = ARITHMETIC_PLUS;
exports.ARITHMETIC_MINUS = ARITHMETIC_MINUS;
exports.ARITHMETIC_TIMES = ARITHMETIC_TIMES;
exports.ARITHMETIC_DIVIDE = ARITHMETIC_DIVIDE;
exports.ARITHMETIC_MODULUS = ARITHMETIC_MODULUS;
exports.STRING_CONCAT = STRING_CONCAT;
exports.STRING_CONCAT_SEPARATOR = STRING_CONCAT_SEPARATOR;
exports.CHAR_LENGTH = CHAR_LENGTH;
exports.STRING_LOWER = STRING_LOWER;
exports.STRING_UPPER = STRING_UPPER;
exports.STRING_SUBSTRING = STRING_SUBSTRING;
exports.STRING_CONTAINS = STRING_CONTAINS;
exports.STRING_LIKE = STRING_LIKE;
exports.STRING_LEFT = STRING_LEFT;
exports.STRING_RIGHT = STRING_RIGHT;
exports.STRING_TRIM = STRING_TRIM;
exports.CAST_BOOL = CAST_BOOL;
exports.CAST_NUMBER = CAST_NUMBER;
exports.CAST_STRING = CAST_STRING;
exports.CAST_LIST = CAST_LIST;
exports.IS_NULL = IS_NULL;
exports.IS_BOOL = IS_BOOL;
exports.IS_NUMBER = IS_NUMBER;
exports.IS_STRING = IS_STRING;
exports.IS_LIST = IS_LIST;
exports.IS_DOCUMENT = IS_DOCUMENT;
exports.NUMBER_FLOOR = NUMBER_FLOOR;
exports.NUMBER_CEIL = NUMBER_CEIL;
exports.NUMBER_ROUND = NUMBER_ROUND;
exports.NUMBER_ABS = NUMBER_ABS;
exports.NUMBER_RAND = NUMBER_RAND;
exports.NUMBER_SQRT = NUMBER_SQRT;
exports.SORT = SORT;
exports.GROUP = GROUP;
exports.LIMIT = LIMIT;
exports.LENGTH = LENGTH;
exports.FIRST = FIRST;
exports.LAST = LAST;
exports.POSITION = POSITION;
exports.NTH = NTH;
exports.REVERSE = REVERSE;
exports.RANGE = RANGE;
exports.UNIQUE = UNIQUE;
exports.UNION = UNION;
exports.UNION_DISTINCT = UNION_DISTINCT;
exports.MINUS = MINUS;
exports.INTERSECTION = INTERSECTION;
exports.MAX = MAX;
exports.MIN = MIN;
exports.SUM = SUM;
exports.AVERAGE = AVERAGE;
exports.MEDIAN = MEDIAN;
exports.VARIANCE_SAMPLE = VARIANCE_SAMPLE;
exports.VARIANCE_POPULATION = VARIANCE_POPULATION;
exports.STDDEV_SAMPLE = STDDEV_SAMPLE;
exports.STDDEV_POPULATION = STDDEV_POPULATION;
exports.GEO_NEAR = GEO_NEAR;
exports.GEO_WITHIN = GEO_WITHIN;
exports.FULLTEXT = FULLTEXT;
exports.GRAPH_PATHS = GRAPH_PATHS;
exports.GRAPH_TRAVERSAL = GRAPH_TRAVERSAL;
exports.GRAPH_TRAVERSAL_TREE = GRAPH_TRAVERSAL_TREE;
exports.GRAPH_EDGES = GRAPH_EDGES;
exports.GRAPH_NEIGHBORS = GRAPH_NEIGHBORS;
exports.NOT_NULL = NOT_NULL;
exports.FIRST_LIST = FIRST_LIST;
exports.FIRST_DOCUMENT = FIRST_DOCUMENT;
exports.PARSE_IDENTIFIER = PARSE_IDENTIFIER;
exports.HAS = HAS;
exports.ATTRIBUTES = ATTRIBUTES;
exports.UNSET = UNSET;
exports.KEEP = KEEP;
exports.MERGE = MERGE;
exports.MERGE_RECURSIVE = MERGE_RECURSIVE;
exports.MATCHES = MATCHES;
exports.PASSTHRU = PASSTHRU;
exports.SLEEP = SLEEP;
exports.FAIL = FAIL;

exports.reload = reloadUserFunctions;

// initialise the query engine
resetEngine();

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}"
// End:

/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global AQL_EXECUTE */
////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, functions
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
var errors = internal.errors;
var jsunity = require("jsunity");
var db = internal.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlFunctionsBruteTestSuite () {
  var c = null;

  var all = [
    "IS_NULL",
    "IS_BOOL",
    "IS_NUMBER",
    "IS_STRING",
    "IS_ARRAY",
    "IS_LIST",
    "IS_OBJECT",
    "IS_DOCUMENT",
    "IS_DATESTRING",
    "TO_NUMBER",
    "TO_STRING",
    "TO_BOOL",
    "TO_ARRAY",
    "TO_LIST",
    "CONCAT",
    "CONCAT_SEPARATOR",
    "CHAR_LENGTH",
    "LOWER",
    "UPPER",
    "SUBSTRING",
    "CONTAINS",
    "LIKE",
    "LEFT",
    "RIGHT",
    "TRIM",
    "LTRIM",
    "RTRIM",
    "FIND_FIRST",
    "FIND_LAST",
    "SPLIT",
    "SUBSTITUTE",
    "MD5",
    "SHA1",
    "RANDOM_TOKEN",
    "FLOOR",
    "CEIL",
    "ROUND",
    "ABS",
    "RAND",
    "SQRT",
    "POW",
    "RANGE",
    "UNION",
    "UNION_DISTINCT",
    "MINUS",
    "INTERSECTION",
    "FLATTEN",
    "LENGTH",
    "COUNT",
    "MIN",
    "MAX",
    "SUM",
    "MEDIAN",
    "PERCENTILE",
    "AVERAGE",
    "AVG",
    "VARIANCE_SAMPLE",
    "VARIANCE_POPULATION",
    "STDDEV_SAMPLE",
    "STDDEV_POPULATION",
    "STDDEV",
    "UNIQUE",
    "SLICE",
    "REVERSE",
    "FIRST",
    "LAST",
    "NTH",
    "POSITION",
    "CALL",
    "APPLY",
    "PUSH",
    "APPEND",
    "POP",
    "SHIFT",
    "UNSHIFT",
    "REMOVE_VALUE",
    "REMOVE_VALUES",
    "REMOVE_NTH",
    "HAS",
    "ATTRIBUTES",
    "VALUES",
    "MERGE",
    "MERGE_RECURSIVE",
    "DOCUMENT",
    "MATCHES",
    "UNSET",
    "UNSET_RECURSIVE",
    "KEEP",
    "TRANSLATE",
    "ZIP",
    "NEAR",
    "WITHIN",
    "WITHIN_RECTANGLE",
    "IS_IN_POLYGON",
    "FULLTEXT",
    "DATE_NOW",
    "DATE_TIMESTAMP",
    "DATE_ISO8601",
    "DATE_DAYOFWEEK",
    "DATE_YEAR",
    "DATE_MONTH",
    "DATE_DAY",
    "DATE_HOUR",
    "DATE_MINUTE",
    "DATE_SECOND",
    "DATE_MILLISECOND",
    "DATE_DAYOFYEAR",
    "DATE_ISOWEEK",
    "DATE_LEAPYEAR",
    "DATE_QUARTER",
    "DATE_DAYS_IN_MONTH",
    "DATE_ADD",
    "DATE_SUBTRACT",
    "DATE_DIFF",
    "DATE_COMPARE",
    "DATE_FORMAT",
    "NOT_NULL",
    "FIRST_LIST",
    "FIRST_DOCUMENT",
    "PARSE_IDENTIFIER",
    "IS_SAME_COLLECTION"
  ];
    
  // find all functions that have parameters
  var funcs = [];
  all.forEach(function(func) {
    var query = "RETURN " + func + "()";
    try {
      AQL_EXECUTE(query);
    } catch (err) {
      var s = String(err);
      var re = s.match(/minimum: (\d+), maximum: (\d+)/);
      if (re) {
        var min = Number(re[1]);
        var max = Number(re[2]);
        
        if (max >= 1000) {
          max = min + 1;
        } 
        if (max > 0) {
          funcs.push({ name: func, min: min, max: max }); 
        }
      }
    }
  });

  var oneArgument = function(func) {
    return func.min === 1;
  };
  
  var twoArguments = function(func) {
    return func.min === 2;
  };

  var skip = function(err) {
    if (!err.hasOwnProperty('errorNum')) {
      return false;
    }
    
    return [
      errors.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code,
      errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code,
      errors.ERROR_QUERY_FUNCTION_NOT_FOUND.code,
      errors.ERROR_QUERY_FUNCTION_NAME_UNKNOWN.code,
      errors.ERROR_GRAPH_NOT_FOUND.code,
      errors.ERROR_GRAPH_INVALID_GRAPH.code,
    ].indexOf(err.errorNum) !== -1;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop("UnitTestsFunctions");
      c = db._create("UnitTestsFunctions");
      c.insert({ _key: "test", value: "test" });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop("UnitTestsFunctions");
    },

    testFunctionsOneWithDoc : function() {
      funcs.filter(oneArgument).forEach(function(func) {
        var query = "FOR doc IN @@collection RETURN " + func.name + "(doc)";
        try {
          AQL_EXECUTE(query, { "@collection" : c.name() });
        } catch (err) {
          if (!skip(err)) {
            throw err;
          }
        }
      });
    },
    
    testFunctionsOneWithDocArray : function() {
      funcs.filter(oneArgument).forEach(function(func) {
        var query = "FOR doc IN @@collection RETURN " + func.name + "([ doc ])";
        try {
          AQL_EXECUTE(query, { "@collection" : c.name() });
        } catch (err) {
          if (!skip(err)) {
            throw err;
          }
        }
      });
    },
    
    testFunctionsOneWithKey : function() {
      funcs.filter(oneArgument).forEach(function(func) {
        var query = "FOR doc IN @@collection RETURN " + func.name + "(doc._key)";
        try {
          AQL_EXECUTE(query, { "@collection" : c.name() });
        } catch (err) {
          if (!skip(err)) {
            throw err;
          }
        }
      });
    },
    
    testFunctionsOneWithId : function() {
      funcs.filter(oneArgument).forEach(function(func) {
        var query = "FOR doc IN @@collection RETURN " + func.name + "(doc._id)";
        try {
          AQL_EXECUTE(query, { "@collection" : c.name() });
        } catch (err) {
          if (!skip(err)) {
            throw err;
          }
        }
      });
    },
    
    testFunctionsOneWithIdArray : function() {
      funcs.filter(oneArgument).forEach(function(func) {
        var query = "FOR doc IN @@collection RETURN " + func.name + "([ doc._id ])";
        try {
          AQL_EXECUTE(query, { "@collection" : c.name() });
        } catch (err) {
          if (!skip(err)) {
            throw err;
          }
        }
      });
    },
    
    testFunctionsTwoWithDoc : function() {
      funcs.filter(twoArguments).forEach(function(func) {
        var query = "FOR doc IN @@collection RETURN " + func.name + "(doc, doc)";
        try {
          AQL_EXECUTE(query, { "@collection" : c.name() });
        } catch (err) {
          if (!skip(err)) {
            throw err;
          }
        }
      });
    },
    
    testFunctionsTwoWithDocArray : function() {
      funcs.filter(twoArguments).forEach(function(func) {
        var query = "FOR doc IN @@collection RETURN " + func.name + "([ doc ], [ doc ])";
        try {
          AQL_EXECUTE(query, { "@collection" : c.name() });
        } catch (err) {
          if (!skip(err)) {
            require("internal").print(query);
            throw err;
          }
        }
      });
    },
    
    testFunctionTwoWithKey : function() {
      funcs.filter(twoArguments).forEach(function(func) {
        var query = "FOR doc IN @@collection RETURN " + func.name + "(doc._key, doc._key)";
        try {
          AQL_EXECUTE(query, { "@collection" : c.name() });
        } catch (err) {
          if (!skip(err)) {
            throw err;
          }
        }
      });
    },
    
    testFunctionsTwoWithId : function() {
      funcs.filter(twoArguments).forEach(function(func) {
        var query = "FOR doc IN @@collection RETURN " + func.name + "(doc._id, doc._id)";
        try {
          AQL_EXECUTE(query, { "@collection" : c.name() });
        } catch (err) {
          if (!skip(err)) {
            throw err;
          }
        }
      });
    },
    
    testFunctionsTwoWithIdArray : function() {
      funcs.filter(twoArguments).forEach(function(func) {
        var query = "FOR doc IN @@collection RETURN " + func.name + "([ doc._id ], [ doc._id ])";
        try {
          AQL_EXECUTE(query, { "@collection" : c.name() });
        } catch (err) {
          if (!skip(err)) {
            throw err;
          }
        }
      });
    },
    
    testFunctionsTwoWithDocId : function() {
      funcs.filter(twoArguments).forEach(function(func) {
        var query = "FOR doc IN @@collection RETURN " + func.name + "(doc, doc._id)";
        try {
          AQL_EXECUTE(query, { "@collection" : c.name() });
        } catch (err) {
          if (!skip(err)) {
            throw err;
          }
        }
      });
    },
    
    testFunctionsTwoWithDocIdArray : function() {
      funcs.filter(twoArguments).forEach(function(func) {
        var query = "FOR doc IN @@collection RETURN " + func.name + "(doc, [ doc._id ])";
        try {
          AQL_EXECUTE(query, { "@collection" : c.name() });
        } catch (err) {
          if (!skip(err)) {
            throw err;
          }
        }
      });
    },
    
    testFunctionsTwoWithIdDoc : function() {
      funcs.filter(twoArguments).forEach(function(func) {
        var query = "FOR doc IN @@collection RETURN " + func.name + "(doc._id, doc)";
        try {
          AQL_EXECUTE(query, { "@collection" : c.name() });
        } catch (err) {
          if (!skip(err)) {
            throw err;
          }
        }
      });
    },
    
    testFunctionsTwoWithIdDocArray : function() {
      funcs.filter(twoArguments).forEach(function(func) {
        var query = "FOR doc IN @@collection RETURN " + func.name + "([ doc._id ], doc)";
        try {
          AQL_EXECUTE(query, { "@collection" : c.name() });
        } catch (err) {
          if (!skip(err)) {
            throw err;
          }
        }
      });
    },
    
    testFunctionsTwoWithIdArrayDocArray : function() {
      funcs.filter(twoArguments).forEach(function(func) {
        var query = "FOR doc IN @@collection RETURN " + func.name + "([ doc._id ], [ doc ])";
        try {
          AQL_EXECUTE(query, { "@collection" : c.name() });
        } catch (err) {
          if (!skip(err)) {
            throw err;
          }
        }
      });
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlFunctionsBruteTestSuite);

return jsunity.done();


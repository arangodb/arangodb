////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, edges
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

var jsunity = require("jsunity");
var internal = require("internal");
var ArangoError = require("org/arangodb").ArangoError; 
var EXPLAIN = internal.AQL_EXPLAIN; 
var QUERY = internal.AQL_QUERY; 

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryEdgesTestSuite () {
  var users = null;
  var relations = null;
  var docs = { };

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query) {
    var cursor = QUERY(query, undefined);
    if (cursor instanceof ArangoError) {
      print(query, cursor.errorMessage);
    }
    assertFalse(cursor instanceof ArangoError);
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query, isFlat) {
    var result = executeQuery(query).getRows();
    var results = [ ];

    for (var i in result) {
      if (!result.hasOwnProperty(i)) {
        continue;
      }

      var row = result[i];
      if (isFlat) {
        results.push(row);
      } 
      else {
        var keys = [ ];
        for (var k in row) {
          if (row.hasOwnProperty(k) && k != '_id' && k != '_rev' && k != '_key') {
            keys.push(k);
          }
        }
       
        keys.sort();
        var resultRow = { };
        for (var k in keys) {
          if (keys.hasOwnProperty(k)) {
            resultRow[keys[k]] = row[keys[k]];
          }
        }
        results.push(resultRow);
      }
    }

    return results;
  }


  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop("UnitTestsAhuacatlUsers");
      internal.db._drop("UnitTestsAhuacatlUserRelations");

      users = internal.db._create("UnitTestsAhuacatlUsers");
      relations = internal.db._createEdgeCollection("UnitTestsAhuacatlUserRelations");

      docs["John"] = users.save({ "id" : 100, "name" : "John" });
      docs["Fred"] = users.save({ "id" : 101, "name" : "Fred" });
      docs["Jacob"] = users.save({ "id" : 102, "name" : "Jacob" });
      docs["Self"] = users.save({ "id" : 103, "name" : "Self" });
      docs["Multi"] = users.save({ "id" : 104, "name" : "Multi" });
      docs["Multi1"] = users.save({ "id" : 105, "name" : "Multi1" });
      docs["Multi2"] = users.save({ "id" : 106, "name" : "Multi2" });
      docs["Multi3"] = users.save({ "id" : 107, "name" : "Multi3" });

      relations.save(docs["John"], docs["Fred"], { what: "John->Fred" });
      relations.save(docs["Fred"], docs["Jacob"], { what: "Fred->Jacob" });
      relations.save(docs["Self"], docs["Self"], { what: "Self->Self" });
      relations.save(docs["Multi"], docs["Multi1"], { what: "Multi->Multi1" });
      relations.save(docs["Multi"], docs["Multi2"], { what: "Multi->Multi2" });
      relations.save(docs["Multi"], docs["Multi3"], { what: "Multi->Multi3" });
      relations.save(docs["Multi2"], docs["Multi3"], { what: "Multi2->Multi3" });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop("UnitTestsAhuacatlUsers");
      internal.db._drop("UnitTestsAhuacatlUserRelations");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the explain output
////////////////////////////////////////////////////////////////////////////////

    testFromQueryExplain : function () {
      var actual = EXPLAIN("FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["John"]._id +"\" RETURN { \"from\" : r._from, \"to\" : r._to }");
      assertEqual("index", actual[0].expression.extra.accessType);
      assertEqual("edge", actual[0].expression.extra.index.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the explain output
////////////////////////////////////////////////////////////////////////////////

    testToQueryExplain : function () {
      var actual = EXPLAIN("FOR r IN UnitTestsAhuacatlUserRelations FILTER r._to == \"" + docs["Fred"]._id +"\" RETURN { \"from\" : r._from, \"to\" : r._to }");
      assertEqual("index", actual[0].expression.extra.accessType);
      assertEqual("edge", actual[0].expression.extra.index.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the explain output
////////////////////////////////////////////////////////////////////////////////

    testFromToQueryExplain : function () {
      var actual = EXPLAIN("FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["John"]._id +"\" && r._to == \"" + docs["Fred"]._id + "\" RETURN { \"from\" : r._from, \"to\" : r._to }");
      assertEqual("index", actual[0].expression.extra.accessType);
      assertEqual("edge", actual[0].expression.extra.index.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the explain output
////////////////////////////////////////////////////////////////////////////////

    testFromToQuerySelfExplain : function () {
      var actual = EXPLAIN("FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["Self"]._id +"\" && r._to == \"" + docs["Self"]._id + "\" RETURN { \"from\" : r._from, \"to\" : r._to }");
      assertEqual("index", actual[0].expression.extra.accessType);
      assertEqual("edge", actual[0].expression.extra.index.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from
////////////////////////////////////////////////////////////////////////////////

    testFromQuerySingle1 : function () {
      var actual = getQueryResults("FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["John"]._id +"\" RETURN { \"from\" : r._from, \"to\" : r._to }");
      assertEqual(1, actual.length);

      assertEqual(docs["John"]._id, actual[0]["from"]);
      assertEqual(docs["Fred"]._id, actual[0]["to"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from
////////////////////////////////////////////////////////////////////////////////

    testFromQuerySingle2 : function () {
      var actual = getQueryResults("FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["Fred"]._id +"\" RETURN { \"from\" : r._from, \"to\" : r._to }");
      assertEqual(1, actual.length);

      assertEqual(docs["Fred"]._id, actual[0]["from"]);
      assertEqual(docs["Jacob"]._id, actual[0]["to"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from
////////////////////////////////////////////////////////////////////////////////

    testFromQuerySingle3 : function () {
      var actual = getQueryResults("FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["Jacob"]._id +"\" RETURN { \"from\" : r._from, \"to\" : r._to }");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from
////////////////////////////////////////////////////////////////////////////////

    testFromQuerySingleSelf : function () {
      var actual = getQueryResults("FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["Self"]._id +"\" RETURN { \"from\" : r._from, \"to\" : r._to }");
      assertEqual(1, actual.length);

      assertEqual(docs["Self"]._id, actual[0]["from"]);
      assertEqual(docs["Self"]._id, actual[0]["to"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _to
////////////////////////////////////////////////////////////////////////////////

    testToQuerySingle1 : function () {
      var actual = getQueryResults("FOR r IN UnitTestsAhuacatlUserRelations FILTER r._to == \"" + docs["Fred"]._id +"\" RETURN { \"from\" : r._from, \"to\" : r._to }");
      assertEqual(1, actual.length);

      assertEqual(docs["John"]._id, actual[0]["from"]);
      assertEqual(docs["Fred"]._id, actual[0]["to"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _to
////////////////////////////////////////////////////////////////////////////////

    testToQuerySingle2 : function () {
      var actual = getQueryResults("FOR r IN UnitTestsAhuacatlUserRelations FILTER r._to == \"" + docs["Jacob"]._id +"\" RETURN { \"from\" : r._from, \"to\" : r._to }");
      assertEqual(1, actual.length);

      assertEqual(docs["Fred"]._id, actual[0]["from"]);
      assertEqual(docs["Jacob"]._id, actual[0]["to"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _to
////////////////////////////////////////////////////////////////////////////////

    testToQuerySingle3 : function () {
      var actual = getQueryResults("FOR r IN UnitTestsAhuacatlUserRelations FILTER r._to == \"" + docs["John"]._id +"\" RETURN { \"from\" : r._from, \"to\" : r._to }");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _to
////////////////////////////////////////////////////////////////////////////////

    testToQuerySingleSelf : function () {
      var actual = getQueryResults("FOR r IN UnitTestsAhuacatlUserRelations FILTER r._to == \"" + docs["Self"]._id +"\" RETURN { \"from\" : r._from, \"to\" : r._to }");
      assertEqual(1, actual.length);

      assertEqual(docs["Self"]._id, actual[0]["from"]);
      assertEqual(docs["Self"]._id, actual[0]["to"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from and _to
////////////////////////////////////////////////////////////////////////////////

    testFromToQuerySingle1 : function () {
      var actual = getQueryResults("FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["John"]._id +"\" && r._to == \"" + docs["Fred"]._id + "\" RETURN { \"from\" : r._from, \"to\" : r._to }");
      assertEqual(1, actual.length);

      assertEqual(docs["John"]._id, actual[0]["from"]);
      assertEqual(docs["Fred"]._id, actual[0]["to"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from and _to
////////////////////////////////////////////////////////////////////////////////

    testFromToQuerySingle2 : function () {
      var actual = getQueryResults("FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["Fred"]._id +"\" && r._to == \"" + docs["John"]._id + "\" RETURN { \"from\" : r._from, \"to\" : r._to }");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from and _to
////////////////////////////////////////////////////////////////////////////////

    testFromToQuerySingle3 : function () {
      var actual = getQueryResults("FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["John"]._id +"\" && r._to == \"" + docs["Jacob"]._id + "\" RETURN { \"from\" : r._from, \"to\" : r._to }");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from and _to
////////////////////////////////////////////////////////////////////////////////

    testFromToQueryLinkedSelf : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["Self"]._id +"\" && r._to == \"" + docs["Self"]._id + "\" && r._to == u._id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(1, actual.length);

      assertEqual(docs["Self"]._id, actual[0]["from"]);
      assertEqual(docs["Self"]._id, actual[0]["to"]);
      assertEqual("Self", actual[0]["link"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from
////////////////////////////////////////////////////////////////////////////////

    testFromQueryLinked1 : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["John"]._id +"\" && r._to == u._id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(1, actual.length);

      assertEqual(docs["John"]._id, actual[0]["from"]);
      assertEqual(docs["Fred"]._id, actual[0]["to"]);
      assertEqual("Fred", actual[0]["link"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from
////////////////////////////////////////////////////////////////////////////////

    testFromQueryLinked2 : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["Fred"]._id +"\" && r._to == u._id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(1, actual.length);

      assertEqual(docs["Fred"]._id, actual[0]["from"]);
      assertEqual(docs["Jacob"]._id, actual[0]["to"]);
      assertEqual("Jacob", actual[0]["link"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from
////////////////////////////////////////////////////////////////////////////////

    testFromQueryLinked3 : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["Jacob"]._id +"\" && r._to == u._id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from
////////////////////////////////////////////////////////////////////////////////

    testFromQueryLinkedSelf : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["Self"]._id +"\" && r._to == u._id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(1, actual.length);

      assertEqual(docs["Self"]._id, actual[0]["from"]);
      assertEqual(docs["Self"]._id, actual[0]["to"]);
      assertEqual("Self", actual[0]["link"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from
////////////////////////////////////////////////////////////////////////////////

    testFromQueryLinkedMulti1 : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["Multi"]._id +"\" && r._to == u._id SORT u.id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(3, actual.length);

      assertEqual(docs["Multi"]._id, actual[0]["from"]);
      assertEqual(docs["Multi1"]._id, actual[0]["to"]);
      assertEqual("Multi1", actual[0]["link"]);
      
      assertEqual(docs["Multi"]._id, actual[1]["from"]);
      assertEqual(docs["Multi2"]._id, actual[1]["to"]);
      assertEqual("Multi2", actual[1]["link"]);
      
      assertEqual(docs["Multi"]._id, actual[2]["from"]);
      assertEqual(docs["Multi3"]._id, actual[2]["to"]);
      assertEqual("Multi3", actual[2]["link"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from
////////////////////////////////////////////////////////////////////////////////

    testFromQueryLinkedMulti2 : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["Multi2"]._id +"\" && r._to == u._id SORT u.id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(1, actual.length);

      assertEqual(docs["Multi2"]._id, actual[0]["from"]);
      assertEqual(docs["Multi3"]._id, actual[0]["to"]);
      assertEqual("Multi3", actual[0]["link"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from
////////////////////////////////////////////////////////////////////////////////

    testFromQueryLinkedMulti3 : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["Multi3"]._id +"\" && r._to == u._id SORT u.id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _to
////////////////////////////////////////////////////////////////////////////////

    testToQueryLinked1 : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._to == \"" + docs["Fred"]._id +"\" && r._to == u._id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(1, actual.length);

      assertEqual(docs["John"]._id, actual[0]["from"]);
      assertEqual(docs["Fred"]._id, actual[0]["to"]);
      assertEqual("Fred", actual[0]["link"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _to
////////////////////////////////////////////////////////////////////////////////

    testToQueryLinked2 : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._to == \"" + docs["Jacob"]._id +"\" && r._to == u._id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(1, actual.length);

      assertEqual(docs["Fred"]._id, actual[0]["from"]);
      assertEqual(docs["Jacob"]._id, actual[0]["to"]);
      assertEqual("Jacob", actual[0]["link"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _to
////////////////////////////////////////////////////////////////////////////////

    testToQueryLinked3 : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._to == \"" + docs["John"]._id +"\" && r._to == u._id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _to
////////////////////////////////////////////////////////////////////////////////

    testToQueryLinkedSelf : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._to == \"" + docs["Self"]._id +"\" && r._to == u._id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(1, actual.length);

      assertEqual(docs["Self"]._id, actual[0]["from"]);
      assertEqual(docs["Self"]._id, actual[0]["to"]);
      assertEqual("Self", actual[0]["link"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _to
////////////////////////////////////////////////////////////////////////////////

    testToQueryLinkedMulti1 : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._to == \"" + docs["Multi"]._id +"\" && r._to == u._id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _to
////////////////////////////////////////////////////////////////////////////////

    testToQueryLinkedMulti2 : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._to == \"" + docs["Multi3"]._id +"\" && r._to == u._id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(2, actual.length);

      assertEqual(docs["Multi"]._id, actual[0]["from"]);
      assertEqual(docs["Multi3"]._id, actual[0]["to"]);
      assertEqual("Multi3", actual[0]["link"]);
      
      assertEqual(docs["Multi2"]._id, actual[1]["from"]);
      assertEqual(docs["Multi3"]._id, actual[1]["to"]);
      assertEqual("Multi3", actual[1]["link"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from and _to
////////////////////////////////////////////////////////////////////////////////

    testFromToQueryLinked1 : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["John"]._id +"\" && r._to == \"" + docs["Fred"]._id + "\" && r._to == u._id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(1, actual.length);

      assertEqual(docs["John"]._id, actual[0]["from"]);
      assertEqual(docs["Fred"]._id, actual[0]["to"]);
      assertEqual("Fred", actual[0]["link"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from and _to
////////////////////////////////////////////////////////////////////////////////

    testFromToQueryLinked2 : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["Fred"]._id +"\" && r._to == \"" + docs["John"]._id + "\" && r._to == u._id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from and _to
////////////////////////////////////////////////////////////////////////////////

    testFromToQueryLinked3 : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["John"]._id +"\" && r._to == \"" + docs["Jacob"]._id + "\" && r._to == u._id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(0, actual.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from and _to
////////////////////////////////////////////////////////////////////////////////

    testFromToQueryLinkedSelf : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["Self"]._id +"\" && r._to == \"" + docs["Self"]._id + "\" && r._to == u._id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(1, actual.length);

      assertEqual(docs["Self"]._id, actual[0]["from"]);
      assertEqual(docs["Self"]._id, actual[0]["to"]);
      assertEqual("Self", actual[0]["link"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from and _to
////////////////////////////////////////////////////////////////////////////////

    testFromToQueryLinkedMulti1 : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["Multi"]._id +"\" && r._to == \"" + docs["Multi3"]._id + "\" && r._to == u._id SORT u.id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(1, actual.length);

      assertEqual(docs["Multi"]._id, actual[0]["from"]);
      assertEqual(docs["Multi3"]._id, actual[0]["to"]);
      assertEqual("Multi3", actual[0]["link"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from and _to
////////////////////////////////////////////////////////////////////////////////

    testFromToQueryLinkedMulti2 : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["Multi2"]._id +"\" && r._to == \"" + docs["Multi3"]._id + "\" && r._to == u._id SORT u.id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(1, actual.length);

      assertEqual(docs["Multi2"]._id, actual[0]["from"]);
      assertEqual(docs["Multi3"]._id, actual[0]["to"]);
      assertEqual("Multi3", actual[0]["link"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief checks an edge index query using _from and _to
////////////////////////////////////////////////////////////////////////////////

    testFromToQueryLinkedMulti3 : function () {
      var actual = getQueryResults("FOR u IN UnitTestsAhuacatlUsers FOR r IN UnitTestsAhuacatlUserRelations FILTER r._from == \"" + docs["Multi1"]._id +"\" && r._to == \"" + docs["Multi2"]._id + "\" && r._to == u._id SORT u.id RETURN { \"from\" : r._from, \"to\" : r._to, \"link\" : u.name }");
      assertEqual(0, actual.length);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryEdgesTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

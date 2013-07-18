////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, variable access
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
var helper = require("org/arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryVariablesTestSuite () {
  var users1 = null;
  var users2 = null;
  var airports = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      users1 = [ 
        { "id" : 1, "name" : "Max", "hobbies" : [ { "type" : "swimming" }, { "type" : "skating" } ], "friends": [ { "name" : "Peter", "id" : 1 } ] }, 
        { "id" : 2, "name" : "Vanessa", "hobbies" : [ { "type" : "running" }, { "type" : "cycling" } ], "friends" : [ { "name" : "Peter", "id" : 3 }, { "name" : "Max", "id" : 3 } ] }, 
        { "id" : 3, "name" : "Peter", "hobbies": [ ], "friends" : [ { "name" : "Peter" } ] } 
      ];

      users2 = [ 
        { "id" : 1, "name" : "Max", "address" : { "home" : { "street" : "arango road", "zip" : "abcde", "phone" : [ { "type" : "mobile", "number" : "555-1234567" }, { "type" : "fax", "number" : "555-2345678" } ] }, "work": { "street" : "cantaloupe way", "zip" : "xyzab", "phone" : [ { "type" : "landline", "number" : "555-5555555" } ] } } }, 
        { "id" : 2, "name" : "Vanessa", "address" : { "home" : { "street" : "one-way loop", "zip" : "4e2af", "phone" : [ { "type" : "landline", "number" : "555-4352367" } ] }, "work": { "street" : "workers ave", "zip" : "4e2af", "phone" : [ { "type" : "landline", "number" : "555-2214212" } ] } } }, 
        { "id" : 3, "name" : "Peter", "address" : { "home" : { "street" : "theather drive", "zip" : "99998", "phone" : [ { "type" : "mobile", "number" : "555-9624218" }, { "type" : "fax", "number" : "555-4425742" }, { "type" : "landline", "number" : "555-3485385" } ] } } }
      ];

      airports = [
        { "continent" : { "name" : "Europe", "countries" : [ 
          { "name" : "DE", "airports" : [ { "name" : "CGN" }, { "name" : "DTM" }, { "name" : "DUS" } , { "name" : "MUC" }, { "name" : "FRA" }, { "name" : "TXL" }, { "name" : "THF" } ] },
          { "name" : "UK", "airports" : [ { "name" : "LHR" }, { "name" : "LGW" }, { "name" : "STN" } , { "name" : "LCY" }, { "name" : "MAN" } ] }, 
          { "name" : "FR", "airports" : [ { "name" : "CDG" }, { "name" : "ORY" }, { "name" : "LYN" } , { "name" : "MRS" } ] } 
        ] } },
        { "continent" : { "name" : "America", "countries" : [ 
          { "name" : "US", "airports" : [ { "name" : "LGA" }, { "name" : "JFK" }, { "name" : "SFO" }, { "name" : "JER" }, { "name" : "ATL" } ] },
          { "name" : "CA", "airports" : [ { "name" : "YTO" }, { "name" : "YVR" }, { "name" : "YYC" } ] },
          { "name" : "CO", "airports" : [ { "name" : "BOG" } ] }
        ] } }, 
        { "continent" : { "name" : "Asia", "countries" : [ 
          { "name" : "JP", "airports" : [ { "name" : "NRT" }, { "name" : "HND" } , { "name" : "OKD" }, { "name" : "OKA" } ] }
        ] } } 
      ];

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return an expanded variable
////////////////////////////////////////////////////////////////////////////////

    testListExpansion1 : function () {
      var query = "FOR u IN " + JSON.stringify(users1) + " RETURN { \"name\" : u.name, \"likes\": u.hobbies[*].type }";
      var expected = [
        { "likes" : ["swimming", "skating"], "name" : "Max" }, 
        { "likes" : ["running", "cycling"], "name" : "Vanessa" }, 
        { "likes" : [ ], "name" : "Peter" }
      ]; 
      
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

                         
////////////////////////////////////////////////////////////////////////////////
/// @brief return an expanded variable
////////////////////////////////////////////////////////////////////////////////

    testListExpansion2 : function () {
      var query = "FOR u IN " + JSON.stringify(users1) + " RETURN { \"name\" : u.name, \"likes\": u.hobbies[*].type, \"friends\": u.friends[*].name }";
      var expected = [
        { "friends" : ["Peter"], "likes" : ["swimming", "skating"], "name" : "Max" }, 
        { "friends" : [ "Peter", "Max" ], "likes" : ["running", "cycling"], "name" : "Vanessa" },
        { "friends" : ["Peter"], "likes" : [ ], "name" : "Peter" }
      ];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return an expanded variable
////////////////////////////////////////////////////////////////////////////////

    testListExpansion3 : function () {
      var query = "FOR u IN " + JSON.stringify(users2) + " RETURN { \"uid\" : u.id, \"phones\" : u.address.home.phone[*].number }";
      var expected = [
        { "phones" : ["555-1234567", "555-2345678"], "uid" : 1 }, 
        { "phones" : ["555-4352367"], "uid" : 2 }, 
        { "phones" : ["555-9624218", "555-4425742", "555-3485385"], "uid" : 3 }
      ];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return an expanded variable
////////////////////////////////////////////////////////////////////////////////

    testListExpansion4 : function () {
      var query = "FOR a IN " + JSON.stringify(airports) + " RETURN a.continent.countries[*].name";
      var expected = [["DE", "UK", "FR"], ["US", "CA", "CO"], ["JP"]];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return an expanded variable
////////////////////////////////////////////////////////////////////////////////

    testListExpansion5 : function () {
      var query = "FOR a IN " + JSON.stringify(airports) + " RETURN a.continent.countries[*].airports[0].name";
      var expected = [["CGN", "LHR", "CDG"], ["LGA", "YTO", "BOG"], ["NRT"]];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return an expanded variable
////////////////////////////////////////////////////////////////////////////////

    testListExpansion6 : function () {
      var query = "FOR a IN " + JSON.stringify(airports) + " RETURN a.continent.countries[*].airports[*][0].name";
      var expected = [["CGN", "LHR", "CDG"], ["LGA", "YTO", "BOG"], ["NRT"]];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return an expanded variable
////////////////////////////////////////////////////////////////////////////////

    testListExpansion7 : function () {
      var query = "FOR a IN " + JSON.stringify(airports) + " RETURN a.continent.countries[*].airports[*][1].name";
      var expected = [["DTM", "LGW", "ORY"], ["JFK", "YVR", null], ["HND"]];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a named variable
////////////////////////////////////////////////////////////////////////////////

    testNamedAccess1 : function () {
      var query = "FOR u IN " + JSON.stringify(users2) + " RETURN { \"name\" : u.name, \"addr\" : u.address }";
      var expected = [
        { "addr" : { "home" : { "street" : "arango road", "zip" : "abcde", "phone" : [{ "type" : "mobile", "number" : "555-1234567" }, { "type" : "fax", "number" : "555-2345678" }] }, "work" : { "street" : "cantaloupe way", "zip" : "xyzab", "phone" : [{ "type" : "landline", "number" : "555-5555555" }] } }, "name" : "Max" }, 
        { "addr" : { "home" : { "street" : "one-way loop", "zip" : "4e2af", "phone" : [{ "type" : "landline", "number" : "555-4352367" }] }, "work" : { "street" : "workers ave", "zip" : "4e2af", "phone" : [{ "type" : "landline", "number" : "555-2214212" }] } }, "name" : "Vanessa" }, 
        { "addr" : { "home" : { "street" : "theather drive", "zip" : "99998", "phone" : [{ "type" : "mobile", "number" : "555-9624218" }, { "type" : "fax", "number" : "555-4425742" }, { "type" : "landline", "number" : "555-3485385" }] } }, "name" : "Peter" }
      ];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a named variable
////////////////////////////////////////////////////////////////////////////////

    testNamedAccess2 : function () {
      var query = "FOR u IN " + JSON.stringify(users2) + " RETURN { \"name\" : u.name, \"home\" : u.address.home }";
      var expected = [
        { "home" : { "street" : "arango road", "zip" : "abcde", "phone" : [{ "type" : "mobile", "number" : "555-1234567" }, { "type" : "fax", "number" : "555-2345678" }] }, "name" : "Max" }, 
        { "home" : { "street" : "one-way loop", "zip" : "4e2af", "phone" : [{ "type" : "landline", "number" : "555-4352367" }] }, "name" : "Vanessa" }, 
        { "home" : { "street" : "theather drive", "zip" : "99998", "phone" : [{ "type" : "mobile", "number" : "555-9624218" }, { "type" : "fax", "number" : "555-4425742" }, { "type" : "landline", "number" : "555-3485385" }] }, "name" : "Peter" }
      ];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a named variable
////////////////////////////////////////////////////////////////////////////////

    testNamedAccess3 : function () {
      var query = "FOR u IN " + JSON.stringify(users2) + " RETURN { \"name\" : u.name, \"str\" : u.address.home.street }";
      var expected = [ { "name" : "Max", "str" : "arango road" }, { "name" : "Vanessa", "str" : "one-way loop" }, { "name" : "Peter", "str" : "theather drive" } ];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a named variable
////////////////////////////////////////////////////////////////////////////////

    testNamedAccess4 : function () {
      var query = "FOR a IN " + JSON.stringify(airports) + " RETURN a.continent.name";
      var expected = [ "Europe", "America", "Asia" ];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a numerically indexed variable
////////////////////////////////////////////////////////////////////////////////

    testIndexedAccess1 : function () {
      var query = "FOR u IN " + JSON.stringify(users1) + " RETURN u.hobbies[0]";
      var expected = [ { "type": "swimming" }, { "type" : "running" }, null ];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a numerically indexed variable
////////////////////////////////////////////////////////////////////////////////

    testIndexedAccess2 : function () {
      var query = "FOR u IN " + JSON.stringify(users1) + " RETURN u.hobbies[0].type";
      var expected = [ "swimming", "running", null ];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a numerically indexed variable
////////////////////////////////////////////////////////////////////////////////

    testIndexedAccess3 : function () {
      var query = "FOR u IN " + JSON.stringify(users2) + " RETURN { \"uid\" : u.id, \"phone\" : u.address.home.phone[0] }";
      var expected = [
        { "phone" : { "type" : "mobile", "number" : "555-1234567" }, "uid" : 1 }, 
        { "phone" : { "type" : "landline", "number" : "555-4352367" }, "uid" : 2 }, 
        { "phone" : { "type" : "mobile", "number" : "555-9624218" }, "uid" : 3 }
      ];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief return a numerically indexed variable
////////////////////////////////////////////////////////////////////////////////

    testIndexedAccess4 : function () {
      var query = "FOR u IN " + JSON.stringify(users2) + " RETURN { \"uid\" : u.id, \"phone\" : u.address.home.phone[1] }";
      var expected = [
        { "phone" : { "type" : "fax", "number" : "555-2345678" }, "uid" : 1 }, 
        { "phone" : null, "uid" : 2 }, 
        { "phone" : { "type" : "fax", "number" : "555-4425742" }, "uid" : 3 }
      ];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a numerically indexed variable
////////////////////////////////////////////////////////////////////////////////

    testIndexedAccess5 : function () {
      var query = "FOR u IN " + JSON.stringify(users2) + " RETURN { \"uid\" : u.id, \"phoneType\" : u.address.home.phone[0].type, \"phoneNum\" : u.address.home.phone[0].number }";
      var expected = [
        { "phoneNum" : "555-1234567", "phoneType" : "mobile", "uid" : 1 }, 
        { "phoneNum" : "555-4352367", "phoneType" : "landline", "uid" : 2 },
        { "phoneNum" : "555-9624218", "phoneType" : "mobile", "uid" : 3 }
      ];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a numerically indexed variable
////////////////////////////////////////////////////////////////////////////////

    testIndexedAccess6 : function () {
      var query = "FOR u IN " + JSON.stringify(users2) + " RETURN { \"uid\" : u.id, \"phoneType\" : u.address.home.phone[1].type, \"phoneNum\" : u.address.home.phone[1].number }";
      var expected = [
        { "phoneNum" : "555-2345678", "phoneType" : "fax", "uid" : 1 }, 
        { "phoneNum" : null, "phoneType" : null, "uid" : 2 }, 
        { "phoneNum" : "555-4425742", "phoneType" : "fax", "uid" : 3 }
      ];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a numerically indexed variable
////////////////////////////////////////////////////////////////////////////////

    testIndexedAccess7 : function () {
      var query = "FOR a IN " + JSON.stringify(airports) + " RETURN a.continent.countries[0].airports[0].name";
      var expected = ["CGN", "LGA", "NRT"];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return data using quoted identifiers
////////////////////////////////////////////////////////////////////////////////

    testEscapedAccess : function () {
      var query = "FOR a IN " + JSON.stringify(airports) + " RETURN `a`.`continent`.`countries`[0].`airports`[0].`name`";
      var expected = ["CGN", "LGA", "NRT"];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief collect and return (should omit any temporary variables)
////////////////////////////////////////////////////////////////////////////////

    testTemporaryVariables1 : function () {
      var data = [ 
        { name: "baz" }, 
        { name: "bar" } 
      ];
      var expected = [ 
        { "criteria" : "bar", "g" : [ { "y" : { "test" : "test", "name" : "bar" } } ] }, 
        { "criteria" : "baz", "g" : [ { "y" : { "test" : "test", "name" : "baz" } } ] } 
      ];

      var query = "FOR y IN (FOR x IN " + JSON.stringify(data) + " LET object = (FOR a IN [ '1', '2' ] RETURN a) RETURN { test: \"test\", name: x.name }) COLLECT criteria = y.name INTO g LIMIT 10 RETURN { criteria: criteria, g: g }";
      
      var actual = getQueryResults("LET result = (" + query + ") LIMIT 10 RETURN result");
      assertEqual([ expected ], actual);
      
      actual = getQueryResults(query);
      assertEqual(expected, actual);
     
      // omit creating sub-objects 
      query = "FOR y IN (FOR x IN " + JSON.stringify(data) + " RETURN { test: \"test\", name: x.name }) COLLECT criteria = y.name INTO g LIMIT 10 RETURN { criteria: criteria, g: g }";
      
      actual = getQueryResults("LET result = (" + query + ") LIMIT 10 RETURN result");
      assertEqual([ expected ], actual);
      
      actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief collect and return (should omit any temporary variables)
////////////////////////////////////////////////////////////////////////////////

    testTemporaryVariables2 : function () {
      var data = [ 
        { name: "baz", _id: "id1" }, 
        { name: "bar", _id: "id2" }, 
        { name: "foo", _id: "id3" } 
      ];
      var expected = [ 
        { "criteria" : "yid3", "g" : [ { "y" : { "x" : { "name" : "foo", "_id" : "id3" } } } ] }, 
        { "criteria" : "yid2", "g" : [ { "y" : { "x" : { "name" : "bar", "_id" : "id2" } } } ] }, 
        { "criteria" : "yid1", "g" : [ { "y" : { "x" : { "name" : "baz", "_id" : "id1" } } } ] }
      ];

      var query = "FOR y IN (FOR x IN " + JSON.stringify(data) + " RETURN { x: x }) COLLECT criteria = CONCAT(\"y\", y.x._id) INTO g SORT MAX(g[*].y.x._id) DESC LIMIT 10 RETURN { criteria: criteria, g: g }";
      
      var actual = getQueryResults("LET result = (" + query + ") LIMIT 10 RETURN result");
      assertEqual([ expected ], actual);
      
      actual = getQueryResults(query);
      assertEqual(expected, actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryVariablesTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, cross-collection queries
///
/// @file
///
/// DISCLAIMER
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for cross-collection queries
////////////////////////////////////////////////////////////////////////////////

function ahuacatlSubqueryChaos() {
  const collName = "SubqueryChaos";


  const coinToss = function() {
    return Math.random() < 0.5;
  };
  const randomInt = function(from, to) {
    return from + Math.trunc((to - from) * Math.random());
  };

  const chaosGenerator = function*(variables = []) {
    while (true) {
      var filter = "";
      // toss a coin whether we FILTER, if we have a variable to FILTER on
      if (variables.length > 0 && coinToss()) {
        var variable = Math.trunc(randomInt(0, variables.length));
        filter = "FILTER " + variables[variable] + " < " + randomInt(0, 15);
      }

      // toss a coin whether we LIMIT or COLLECT WITH COUNT INTO count
      if (coinToss()) {
        var limit =
            "LIMIT " +
            randomInt(0, 15) +
            ", " +
            randomInt(0, 15);
        yield { mode: "limit", chaos: `${filter} ${limit}` };
      } else {
        var collect = "COLLECT WITH COUNT INTO counter";
        yield { mode: "collect", chaos: `${filter} ${collect}` };
      }
    }
  };

  const idGenerator = function*() {
    var counter = 0;
    while(true) {
      yield counter;
      counter = counter + 1;
    }
  }();

  return {
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////
    setUp: function() {
      db._drop(collName);
      db._create(collName);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////
    tearDown: function() {
      db._drop(collName);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief checks document function, regular
    ////////////////////////////////////////////////////////////////////////////////
    testSubqueryChaos: function() {
      const chaosGen1 = chaosGenerator(["a", "b", "c"]);
      const chaosGen2 = chaosGenerator(["a", "b"]);
      const chaosGen3 = chaosGenerator(["a"]);

      for (var i = 0; i < 10000; i++) {
        var chaos1 = chaosGen1.next().value;
        if (chaos1.mode == "limit") {
          chaos1.return = "RETURN c";
        } else {
          chaos1.return = "RETURN counter";
        }

        var chaos2 = chaosGen2.next().value;
        if (chaos2.mode == "limit") {
          chaos2.return = "RETURN {b, subqB}";
        } else {
          chaos2.return = "RETURN counter";
        }

        var chaos3 = chaosGen3.next().value;
        if (chaos3.mode == "limit") {
          chaos3.return = "RETURN {a, subqA}";
        } else {
          chaos3.return = "RETURN counter";
        }

        const query = `
        FOR a IN 1..10
          FILTER a < 2
          LET subqA = (FOR b IN 1..10
                         LET subqB = (FOR c IN 1..10
                           ${chaos1.chaos}
                           ${chaos1.return}) // subqB
            ${chaos2.chaos}
            ${chaos2.return}) // subqA
          ${chaos3.chaos}
          ${chaos3.return}`;

        const result1 = db._query(query, {}, {fullCount: true}).toArray();
        const result2 = db
              ._query(query, {}, { fullCount: true, optimizer: { rules: ["-splice-subqueries"] } })
          .toArray();

        assertEqual(result1, result2);
      }
    },

    testSubqueryChaos2: function() {
      var subqueryTotal = 20;

      const query = function(outerVariables) {
        // We have access to all variables in the enclosing scope
        // but we don't want them to leak outside, so we take a copy
        var variables = [...outerVariables];

        const for_variable = "fv" + idGenerator.next().value;
        variables.push(for_variable);

        var my_query = "FOR " + for_variable + " IN 1..100 ";

        // here we could filter by any one of the variables in scope
        // or really filter by any variable in scope in between subqueries

        var nqueries = randomInt(0, Math.min(subqueryTotal, 5));
        subqueryTotal = subqueryTotal - nqueries;
        for (var i=0; i<nqueries; i++) {
          var sqv = "sq" + idGenerator.next().value;
          var sq = query(variables);
          variables.push(sqv);

          my_query = my_query + " LET " + sqv + " = (" + sq + ") ";
        }

        // here we could put a count with collect, but then we have to erase variables in some way. bah.

        my_query = my_query + " RETURN {" + variables.join(",") + "}";

        return my_query;
      };

      assertEqual(1,1);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlSubqueryChaos);

return jsunity.done();

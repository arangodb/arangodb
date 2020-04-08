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

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const helper = require("@arangodb/aql-helper");
const print = require("internal").print;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for cross-collection queries
////////////////////////////////////////////////////////////////////////////////

function ahuacatlSubqueryChaos() {
  const collName = "SubqueryChaos";

  const coinToss = function(bias) {
    if (typeof bias === "number") {
      return Math.random() < Math.min(bias, 1);
    } else if (typeof bias === "undefined") {
      return Math.random() < 0.5;
    }
    throw "coinToss: bias must be a number if given (actual " +
      typeof bias +
      ")";
  };

  const randomInt = function(from, to) {
    if (typeof from === "number") {
      if (typeof to === "undefined") {
        to = from;
        from = 0;
      }
      if (typeof to === "number") {
        return from + Math.trunc((to - from) * Math.random());
      }
      throw "randomInt: either an upper bound or both from and to have to be given and be of type number";
    }
    return 0;
  };

  const randomFilter = function(indent, bias, variables = []) {
    if (variables.length > 0 && coinToss(bias)) {
      var variable = Math.trunc(randomInt(0, variables.length));
      return (
        indent +
        " FILTER " +
        variables[variable] +
        " < " +
        randomInt(0, 15) +
        "\n"
      );
    }
    return "";
  };

  const randomLimit = function(indent, bias) {
    return indent + " LIMIT " + randomInt(20) + "," + randomInt(20) + "\n";
  };

  const randomCollectWithCount = function(indent, bias) {
    if (coinToss(bias)) {
      return indent + " COLLECT WITH COUNT INTO counter \n";
    } else {
      return "";
    }
  };

  const idGeneratorGenerator = function*() {
    var counter = 0;
    while (true) {
      yield counter;
      counter = counter + 1;
    }
  };

  const createChaosQuery = function(numberSubqueries) {
    var subqueryTotal = numberSubqueries;
    const idGenerator = idGeneratorGenerator();
    const collectionSize = 20;

    const query = function(indent, outerVariables) {
      // We have access to all variables in the enclosing scope
      // but we don't want them to leak outside, so we take a copy
      var variables = {
        forVariables: [...outerVariables.forVariables],
        subqueryVariables: [...outerVariables.subqueryVariables]
      };

      const for_variable = "fv" + idGenerator.next().value;
      variables.forVariables.push(for_variable);

      var my_query = "FOR " + for_variable + " IN 1.." + collectionSize + "\n";

      // here we could filter by any one of the variables in scope
      // or really filter by any variable in scope in between subqueries

      my_query = my_query + randomFilter(indent, 0.2, variables);

      var nqueries = randomInt(0, Math.min(subqueryTotal, 3));
      subqueryTotal = subqueryTotal - nqueries;
      for (var i = 0; i < nqueries; i++) {
        var sqv = "sq" + idGenerator.next().value;
        var sq = query(indent + "  ", variables);
        variables.subqueryVariables.push(sqv);

        my_query = my_query + indent + " LET " + sqv + " = (" + sq + ")\n";
        my_query = my_query + randomFilter(indent, 0.4, variables.forVariables);
      }

      my_query = my_query + randomLimit(indent, 0.7);

      collect = randomCollectWithCount(indent, 0.1);
      if (collect !== "") {
        my_query = my_query + collect;
        variables = { forVariables: [], subqueryVariables: ["counter"] };
      } else {
        variables.forVariables = [for_variable];
      }

      // here we could put a count with collect, but then we have to
      // erase variables from there on in. bah.
      my_query =
        my_query +
        indent +
        " RETURN {" +
        [...variables.forVariables, ...variables.subqueryVariables].join(", ") +
        "}";

      return my_query;
    };
    return query("", { forVariables: [], subqueryVariables: [] });
  };

  const createModifyingChaosQuery = function(numberSubqueries) {
    var subqueryTotal = numberSubqueries;
    const idGenerator = idGeneratorGenerator();
    const collectionSize = 20;

    const collectionIdGenerator = (function*() {
      var counter = 0;
      while (true) {
        var cn = "SubqueryChaosCollection" + counter;
        counter = counter + 1;
        yield cn;
      }
    })();

    const query = function(indent, outerVariables) {
      // We have access to all variables in the enclosing scope
      // but we don't want them to leak outside, so we take a copy
      var variables = {
        forVariables: [...outerVariables.forVariables],
        subqueryVariables: [...outerVariables.subqueryVariables],
        collectionNames: [...outerVariables.collectionNames]
      };

      const for_variable = "fv" + idGenerator.next().value;
      variables.forVariables.push(for_variable);

      const collection = collectionIdGenerator.next().value;
      variables.collectionNames.push(collection);
      var my_query = "FOR " + for_variable + " IN " + collection + " \n";

      // here we could filter by any one of the variables in scope
      // or really filter by any variable in scope in between subqueries

      my_query = my_query + randomFilter(indent, 0.2, variables);

      var nqueries = randomInt(0, Math.min(subqueryTotal, 3));
      subqueryTotal = subqueryTotal - nqueries;
      for (var i = 0; i < nqueries; i++) {
        var sqv = "sq" + idGenerator.next().value;
        var sq = query(indent + "  ", variables);
        variables.subqueryVariables.push(sqv);

        my_query = my_query + indent + " LET " + sqv + " = (" + sq + ")\n";
        my_query = my_query + randomFilter(indent, 0.4, variables.forVariables);
      }

      my_query = my_query + indent + " UPSERT {value: fv0} INSERT {value: 2} UPDATE {value: 15, updated: true} IN " + collection + "\n";

      my_query = my_query + randomLimit(indent, 0.7);

      collect = randomCollectWithCount(indent, 0.1);
      if (collect !== "") {
        my_query = my_query + collect;
        variables = { forVariables: [], subqueryVariables: ["counter"] };
      } else {
        variables.forVariables = [for_variable];
      }

      // here we could put a count with collect, but then we have to
      // erase variables from there on in. bah.
      my_query =
        my_query +
        indent +
        " RETURN {" +
        [...variables.forVariables, ...variables.subqueryVariables].join(", ") +
        "}";

      return {collectionNames: variables.collectionNames, queryString: my_query};
    };
    return query("", { forVariables: [], subqueryVariables: [], collectionNames: [] })
  };

  const specificQueries = {
    q1: `FOR x IN 1..10
           LET sub = (FOR y in 1..10 RETURN y)
           LIMIT 3, 0
           RETURN sub`,
    q2: `FOR x IN 1..10
           LET sub = (FOR y in 1..10 RETURN y)
           LIMIT 3, 1
           RETURN sub`,
    q3: `FOR fv62 IN 1..100
          LET sq63 = (FOR fv64 IN 1..100
            LET sq65 = (FOR fv66 IN 1..100
              LET sq67 = (FOR fv68 IN 1..100
                          LIMIT 11,19
                          RETURN {fv68})
              LET sq69 = (FOR fv70 IN 1..100
                LET sq71 = (FOR fv72 IN 1..100
                            LIMIT 12,10
                            RETURN {fv72, sq67})
                         FILTER fv70 < 7
                         LIMIT 11,0
                         RETURN {fv70, sq67, sq71})
                  LIMIT 2,6
                  RETURN {fv66, sq67, sq69})
            LET sq73 = (FOR fv74 IN 1..100
                        LIMIT 15,15
                        RETURN {fv74, sq65})
            FILTER fv62 < 13
            LIMIT 4,13
            COLLECT WITH COUNT INTO counter
            RETURN {counter})
          LIMIT 6,19
          RETURN {fv62, sq63}`
  };
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

    testSpecificQueries: function() {
      for (const [key, value] of Object.entries(specificQueries)) {
        const result1 = db._query(value, {}, {}).toArray();
        const result2 = db
          ._query(value, {}, { optimizer: { rules: ["-splice-subqueries"] } })
          .toArray();

        require("internal").print("spliced:     " + result1);
        require("internal").print("non-spliced: " + result2);
        assertEqual(result1, result2);
      }
    },

    _testSubqueryChaos: function() {
      for (var i = 0; i < 50; i++) {
        const randomQuery = createChaosQuery(7);

        require("internal").print("\n\n" + randomQuery);

        require("internal").print("with splicing: ");
        const result1 = db._query(randomQuery, {}, {}).toArray();
        require("internal").print("without splicing: ");
        const result2 = db
          ._query(
            randomQuery,
            {},
            { fullCount: true, optimizer: { rules: ["-splice-subqueries"] } }
          )
          .toArray();

        assertEqual(result1, result2);
      }
    },

    testSubqueryModificationChaos: function() {
      const randomQuery = createModifyingChaosQuery(7);

      require("internal").print("\n\n" + JSON.stringify(randomQuery));
      require("internal").print("with splicing: ");
      for(cn of randomQuery.collectionNames) {
        db._drop(cn);
        db._createDocumentCollection(cn);
        db._query(`FOR i IN 1..100 INSERT { value: i } INTO ${cn}`);
      }
      const result1 = db._query(randomQuery, {}, {}).toArray();

      require("internal").print("without splicing: ");
      for(cn of randomQuery.collectionNames) {
        db._drop(cn);
        db._createDocumentCollection(cn);
        db._query(`FOR i IN 1..100 INSERT { value: i } INTO ${cn}`);
      }
      const result2 = db
            ._query(
              randomQuery,
              {},
              { fullCount: true, optimizer: { rules: ["-splice-subqueries"] } }
            )
            .toArray();

      for(cn of randomQuery.collectionNames) {
        db._drop(cn);
      }

      assertEqual(result1, result2);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlSubqueryChaos);

return jsunity.done();

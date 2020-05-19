/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */

////////////////////////////////////////////////////////////////////////////////
///
/// Fuzzing tests for nested subquery execution. Generates random nested subqueries
/// and then runs spliced subqueries against "old style" subqueries and compares
/// results.
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
const _ = require("lodash");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for cross-collection queries
////////////////////////////////////////////////////////////////////////////////

// This is a seedable RandomNumberGenerator
// it is not operfect for Random numbers,
// but good enough for what we are doing here
function* randomNumberGenerator (seed) {
  print("SEEDABLE RANDOM USING SEED: " + seed);
  while (true) {
    const nextVal = Math.cos(seed++) * 10000;
    yield nextVal - Math.floor(nextVal);
  }
};

var theRandomNumberGenerator = [];

const myRandomNumberGenerator = function() {
  // return Math.random();
  return theRandomNumberGenerator.next().value;
};

function ahuacatlSubqueryChaos() {
  const collName = "SubqueryChaos";

  const coinToss = function(bias) {
    if (typeof bias === "number") {
      return myRandomNumberGenerator() < Math.min(bias, 1);
    } else if (typeof bias === "undefined") {
      return myRandomNumberGenerator() < 0.5;
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
        return from + Math.trunc((to - from) * myRandomNumberGenerator());
      }
      throw "randomInt: either an upper bound or both from and to have to be given and be of type number";
    }
    return 0;
  };

  const pickRandomElement = function(list) {
    if (list.length > 0) {
      return list[randomInt(list.length)];
    } else {
      throw "Cannot pick random element of empty list";
      return undefined;
    }
  };

  // chooses a random element from list,
  // removes it from the list, and returns it.
  const popRandomElement = function(list) {
    if (list.length > 0) {
      const idx = randomInt(list.length);
      return list.splice(idx, 1);
    } else {
      throw "Cannot pick random element of empty list";
      return undefined;
    }
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

  const randomUpsert = function(indent, bias, variables, collectionName) {
    if (coinToss(bias)) {
      return (
        indent +
        " UPSERT {value: " +
        pickRandomElement(variables) +
        " } " +
        " INSERT {value: " +
        randomInt(100) +
        " } " +
        " UPDATE {value: " +
        randomInt(100) +
        ", updated: true} IN " +
        collectionName +
        "\n"
      );
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

      my_query =
        my_query +
        indent +
        " RETURN {" +
        [...variables.forVariables, ...variables.subqueryVariables].join(", ") +
        "}";

      return my_query;
    };
    return {
      collectionNames: [],
      queryString: query("", { forVariables: [], subqueryVariables: [] })
    };
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

      my_query = my_query + randomFilter(indent, 0.2, variables);

      var nqueries = randomInt(0, Math.min(subqueryTotal, 3));
      subqueryTotal = subqueryTotal - nqueries;
      for (var i = 0; i < nqueries; i++) {
        var sqv = "sq" + idGenerator.next().value;
        var sq = query(indent + "  ", variables);
        variables.subqueryVariables.push(sqv);
        variables.collectionNames = sq.collectionNames;

        my_query =
          my_query + indent + " LET " + sqv + " = (" + sq.queryString + ")\n";
        my_query = my_query + randomFilter(indent, 0.4, variables.forVariables);
      }

      // at the moment we only modify the current collection, as we otherwise
      // run into problems with generating queries that try to access data after
      // modification
      my_query =
        my_query +
        randomUpsert(indent, 0.3, variables.forVariables, collection);

      my_query = my_query + randomLimit(indent, 0.7);

      collect = randomCollectWithCount(indent, 0.1);
      if (collect !== "") {
        my_query = my_query + collect;
        variables = {
          forVariables: [],
          collectionNames: variables.collectionNames,
          subqueryVariables: ["counter"]
        };
      } else {
        variables.forVariables = [for_variable];
      }

      const returns = [
        ...variables.forVariables,
        ...variables.subqueryVariables
      ]
        .map(x => x + ": UNSET_RECURSIVE(" + x + ',"_rev", "_id", "_key")')
        .join(", ");

      my_query = my_query + indent + " RETURN {" + returns + "}";

      return {
        collectionNames: variables.collectionNames,
        queryString: my_query
      };
    };
    return query("", {
      forVariables: [],
      subqueryVariables: [],
      collectionNames: [],
      modifiableCollectionNames: []
    });
  };

  testQuery = function(query) {
    print(`testing query: ${query.queryString}`);
    for (cn of query.collectionNames) {
      db._drop(cn);
      db._createDocumentCollection(cn);
      db._query(`FOR i IN 1..100 INSERT { value: i } INTO ${cn}`);
    }
    const result1 = db._query(query.queryString, {}, {}).toArray();

    for (cn of query.collectionNames) {
      db._drop(cn);
      db._createDocumentCollection(cn);
      db._query(`FOR i IN 1..100 INSERT { value: i } INTO ${cn}`);
    }
    const result2 = db
      ._query(
        query.queryString,
        {},
        { fullCount: true, optimizer: { rules: ["-splice-subqueries"] } }
      )
      .toArray();

    for (cn of query.collectionNames) {
      db._drop(cn);
    }

    if (!_.isEqual(result1, result2)) {
      throw `Results of query
	${query.queryString}
        with subquery splicing:
	${JSON.stringify(result1)}
        without subquery splicing:
        ${JSON.stringify(result2)}
	do not match!`;
    }
  };

  const specificQueries = {
    q1: {
      queryString: `FOR x IN 1..10
           LET sub = (FOR y in 1..10 RETURN y)
           LIMIT 3, 0
           RETURN sub`,
      collectionNames: []
    },
    q2: {
      queryString: `FOR x IN 1..10
           LET sub = (FOR y in 1..10 RETURN y)
           LIMIT 3, 1
           RETURN sub`,
      collectionNames: []
    },
    q3: {
      queryString: `FOR fv62 IN 1..100
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
          RETURN {fv62, sq63}`,
      collectionNames: []
    },
    q4: {
      queryString: `FOR fv0 IN 1..20
           LET sq1 = (FOR fv2 IN 1..20
             LET sq3 = (FOR fv4 IN 1..20
               LET sq5 = (FOR fv6 IN 1..20
                 LIMIT 2,12
                 COLLECT WITH COUNT INTO counter 
                 RETURN {counter})
               LIMIT 4,0
               COLLECT WITH COUNT INTO counter 
               RETURN {counter})
             FILTER fv2 < 1
             LIMIT 18,8
             RETURN {fv2, sq3})
           LET sq7 = (FOR fv8 IN 1..20
             LET sq9 = (FOR fv10 IN 1..20
               LET sq11 = (FOR fv12 IN 1..20
                 LIMIT 7,3
                 RETURN {fv12, sq1})
               FILTER fv0 < 9
               LIMIT 2,6
               RETURN {fv10, sq1, sq11})
             LIMIT 17,15
             RETURN {fv8, sq1, sq9})
           LIMIT 13,14
           RETURN {fv0, sq1, sq7}`,
      collectionNames: []
    }
  };
  return {
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////
    setUp: function() {
      db._drop(collName);
      db._create(collName);

      theRandomNumberGenerator = randomNumberGenerator(Math.floor(50000 * Math.random()));
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////
    tearDown: function() {
      db._drop(collName);
    },

    testSpecificQueries: function() {
      for (const [key, value] of Object.entries(specificQueries)) {
        testQuery(value);
      }
    },

    testSomeSubqueryChaos: function() {
      for (var i = 0; i < 1000; i++) {
        testQuery(createChaosQuery(7));
      }
    },

    testSomeSubqueryModificationChaos: function() {
      for (var i = 0; i < 1000; i++) {
        testQuery(createModifyingChaosQuery(7));
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlSubqueryChaos);

return jsunity.done();

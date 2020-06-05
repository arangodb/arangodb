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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
///
/// This module is aimed at testing varying interactions between subquery, limit
/// and collect by generating random queries.
///
/// To make debugging not too annoying, we use a seeded random number generator
/// and print the seed with every test. This means that, should you be faced with
/// the thankless task of debugging a query that has been produced by this code,
/// all you have to do is to run `testQueryWithSeed` or `testModifyingQueryWithSeed`
/// with the seed that the test suite printed out for you and you'll get the exact
/// failing query.
///
/// the two exported functions in this module take an object with options. The
/// following options are understood:
///
/// seed:             an integer, the random seed to use
/// numberSubqueries: an integer, how many subqueries to generate
/// printQuery:       a boolean, whether to print the query string *before* running
///                   any test
/// explainQuery:     explain the query before running it. Note that this option
///                   causes the query to be explained twice: once with subquery
///                   splicing, and once without.
///
///
const db = require("@arangodb").db;
const _ = require("lodash");

// This is a seedable RandomNumberGenerator
// it is not operfect for Random numbers,
// but good enough for what we are doing here
function randomNumberGeneratorGenerator(seed) {
  const rng = (function* (seed) {
    while (true) {
      const nextVal = Math.cos(seed++) * 10000;
      yield nextVal - Math.floor(nextVal);
    }
  })(seed);

  return function () {
    return rng.next().value;
  };
}

function coinToss(generator, bias) {
  if (typeof bias === "number") {
    return generator() < Math.min(bias, 1);
  } else if (typeof bias === "undefined") {
    return generator() < 0.5;
  }
  throw "coinToss: bias must be a number if given (actual " + typeof bias + ")";
}

function randomInt(generator, from, to) {
  if (typeof from === "number") {
    if (typeof to === "undefined") {
      to = from;
      from = 0;
    }
    if (typeof to === "number") {
      return from + Math.trunc((to - from) * generator());
    }
    throw "randomInt: either an upper bound or both from and to have to be given and be of type number";
  }
  return 0;
}

function pickRandomElement(generator, list) {
  if (list.length > 0) {
    return list[randomInt(generator, list.length)];
  } else {
    throw "Cannot pick random element of empty list";
    return undefined;
  }
}

// chooses a random element from list,
// removes it from the list, and returns it.
function popRandomElement(generator, list) {
  if (list.length > 0) {
    const idx = randomInt(generator, list.length);
    return list.splice(idx, 1);
  } else {
    throw "Cannot pick random element of empty list";
    return undefined;
  }
}

function randomFilter(generator, indent, bias, variables = []) {
  if (variables.length > 0 && coinToss(generator, bias)) {
    var variable = Math.trunc(randomInt(generator, 0, variables.length));
    return (
      indent +
      " FILTER " +
      variables[variable] +
      " < " +
      randomInt(generator, 0, 15) +
      "\n"
    );
  }
  return "";
}

function randomLimit(generator, indent, bias) {
  return (
    indent +
    " LIMIT " +
    randomInt(generator, 20) +
    "," +
    randomInt(generator, 20) +
    "\n"
  );
}

function randomCollectWithCount(generator, indent, bias) {
  if (coinToss(generator, bias)) {
    return indent + " COLLECT WITH COUNT INTO counter \n";
  } else {
    return "";
  }
}

function randomUpsert(generator, indent, bias, variables, collectionName) {
  if (coinToss(generator, bias)) {
    const variable =  pickRandomElement(generator, variables)
    return `${indent} UPSERT { value: ${variable}.value } INSERT { value: ${variable}.value } UPDATE {updated: true} IN ${collectionName}\n`;
  } else {
    return "";
  }
}

function* idGeneratorGenerator() {
  var counter = 0;
  while (true) {
    yield counter;
    counter = counter + 1;
  }
}

function createChaosQuery(seed, numberSubqueries) {
  var subqueryTotal = numberSubqueries;
  const idGenerator = idGeneratorGenerator();
  const randomGenerator = randomNumberGeneratorGenerator(seed);
  const collectionSize = 20;

  const query = function (indent, outerVariables) {
    // We have access to all variables in the enclosing scope
    // but we don't want them to leak outside, so we take a copy
    var variables = {
      forVariables: [...outerVariables.forVariables],
      subqueryVariables: [...outerVariables.subqueryVariables],
    };

    const for_variable = "fv" + idGenerator.next().value;
    variables.forVariables.push(for_variable);

    var my_query = "FOR " + for_variable + " IN 1.." + collectionSize + "\n";

    my_query = my_query + randomFilter(randomGenerator, indent, 0.2, variables);

    var nqueries = randomInt(randomGenerator, 0, Math.min(subqueryTotal, 3));
    subqueryTotal = subqueryTotal - nqueries;
    for (var i = 0; i < nqueries; i++) {
      var sqv = "sq" + idGenerator.next().value;
      var sq = query(indent + "  ", variables);
      variables.subqueryVariables.push(sqv);

      my_query = my_query + indent + " LET " + sqv + " = (" + sq + ")\n";
      my_query =
        my_query + randomFilter(randomGenerator, 0.4, variables.forVariables);
    }

    my_query = my_query + randomLimit(randomGenerator, indent, 0.7);

    collect = randomCollectWithCount(randomGenerator, indent, 0.1);
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
    queryString: query("", { forVariables: [], subqueryVariables: [] }),
  };
}

function createModifyingChaosQuery(seed, numberSubqueries) {
  var subqueryTotal = numberSubqueries;
  const idGenerator = idGeneratorGenerator();
  const randomGenerator = randomNumberGeneratorGenerator(seed);
  const collectionSize = 20;

  const collectionIdGenerator = (function* () {
    var counter = 0;
    while (true) {
      var cn = "SubqueryChaosCollection" + counter;
      counter = counter + 1;
      yield cn;
    }
  })();

  const query = function (indent, outerVariables) {
    var variables = {
      forVariables: [...outerVariables.forVariables],
      subqueryVariables: [...outerVariables.subqueryVariables],
      collectionNames: [...outerVariables.collectionNames],
    };

    const for_variable = "fv" + idGenerator.next().value;
    variables.forVariables.push(for_variable);

    const collection = collectionIdGenerator.next().value;
    variables.collectionNames.push(collection);
    var my_query = "FOR " + for_variable + " IN " + collection + " \n";

    my_query = my_query + randomFilter(randomGenerator, indent, 0.2, variables);

    var nqueries = randomInt(randomGenerator, 0, Math.min(subqueryTotal, 3));
    subqueryTotal = subqueryTotal - nqueries;
    for (var i = 0; i < nqueries; i++) {
      var sqv = "sq" + idGenerator.next().value;
      var sq = query(indent + "  ", variables);
      variables.subqueryVariables.push(sqv);
      variables.collectionNames = sq.collectionNames;

      my_query =
        my_query + indent + " LET " + sqv + " = (" + sq.queryString + ")\n";
      my_query =
        my_query +
        randomFilter(randomGenerator, indent, 0.4, variables.forVariables);
    }

    // at the moment we only modify the current collection, as we otherwise
    // run into problems with generating queries that try to access data after
    // modification
    my_query =
      my_query +
      randomUpsert(
        randomGenerator,
        indent,
        0.3,
        variables.forVariables,
        collection
      );

    my_query = my_query + randomLimit(randomGenerator, indent, 0.7);

    collect = randomCollectWithCount(randomGenerator, indent, 0.1);
    if (collect !== "") {
      my_query = my_query + collect;
      variables = {
        forVariables: [],
        collectionNames: variables.collectionNames,
        subqueryVariables: ["counter"],
      };
    } else {
      variables.forVariables = [for_variable];
    }

    const returns = [...variables.forVariables, ...variables.subqueryVariables]
      .map((x) => x + ": UNSET_RECURSIVE(" + x + ',"_rev", "_id", "_key")')
      .join(", ");

    my_query = my_query + indent + " RETURN {" + returns + "}";

    return {
      collectionNames: variables.collectionNames,
      queryString: my_query,
    };
  };
  return query("", {
    forVariables: [],
    subqueryVariables: [],
    collectionNames: [],
    modifiableCollectionNames: [],
  });
}

function runQuery(query, queryOptions, testOptions) {
  /* Create collections required for this query */
  for (cn of query.collectionNames) {
    db._drop(cn);
    db._createDocumentCollection(cn);
    db._query(`FOR i IN 1..10 INSERT { value: i } INTO ${cn}`);
  }

  if (testOptions.explainQuery) {
    db._explain(query.queryString, {}, queryOptions);
  }

  /* Run query with all optimizations */
  const result = db._query(query.queryString, {}, queryOptions).toArray();

  for (cn of query.collectionNames) {
    db._drop(cn);
  }

  /* Cleanup */
  return result;
}

function testQuery(query, testOptions) {
  /* Print query string */
  if (testOptions.printQuery === true) {
    print(`testing query: ${query.queryString}`);
  }

  /* Run query with all optimizations */
  const result1 = runQuery(query, { }, testOptions);

  /* Run query without subquery splicing */
  const result2 = runQuery(
    query,
    { fullCount: true, optimizer: { rules: ["-splice-subqueries"] } },
    testOptions
  );

  if (!_.isEqual(result1, result2)) {
    const msg = `Results of query
	${query.queryString}
        with subquery splicing:
	${JSON.stringify(result1)}
        without subquery splicing:
        ${JSON.stringify(result2)}
	do not match!`;

    if (testOptions.throwOnMismatch) {
      throw msg;
    } else {
      print(msg);
    }
  }
}

function testQueryWithSeed(testOptions) {
  if (testOptions.showReproduce) {
    print(
      `require("@arangodb/test-generators/subquery-chaos-test").testQueryWithSeed(${JSON.stringify(
        testOptions
      )});`
    );
  }
  const query = createChaosQuery(
    testOptions.seed,
    testOptions.numberSubqueries
  );

  testQuery(query, testOptions);
}

function testModifyingQueryWithSeed(testOptions) {
  if (testOptions.showReproduce) {
    print(
      `require("@arangodb/test-generators/subquery-chaos-test").testModifyingQueryWithSeed(${JSON.stringify(
        testOptions
      )});`
    );
  }
  const query = createModifyingChaosQuery(
    testOptions.seed,
    testOptions.numberSubqueries
  );

  testQuery(query, testOptions);
}

exports.testQuery = testQuery;
exports.testQueryWithSeed = testQueryWithSeed;
exports.testModifyingQueryWithSeed = testModifyingQueryWithSeed;

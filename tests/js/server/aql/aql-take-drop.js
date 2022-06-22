/*jshint globalstrict:true, strict:true, esnext: true */

'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const helper = require('@arangodb/aql-helper');
const getQueryResults = helper.getQueryResults;
const _ = require('lodash');
const arangodb = require('@arangodb');
const db = arangodb.db;

const WHILE = 'WHILE';
const UNTIL = 'UNTIL';

const aqlTakeDropWhileUntilSyntaxSuite = function () {
  return {
    testTakeWhile: function () {
      const cases = [
        [`FOR i IN 1..10 TAKE WHILE i < 5 RETURN i`, _.range(1, 5)],
        [`FOR i IN 1..10 TAKE WHILE i != 5 RETURN i`, _.range(1, 5)],
        [`FOR i IN 1..10 TAKE WHILE ! (i == 5) RETURN i`, _.range(1, 5)],
        [`FOR i IN 1..10 TAKE WHILE NOT (i == 5) RETURN i`, _.range(1, 5)],
        [`FOR i IN 1..10 TAKE WHILE i < 5 AND i < 7 RETURN i`, _.range(1, 5)],
        [`FOR i IN 1..10 TAKE WHILE i < 7 AND i < 5 RETURN i`, _.range(1, 5)],
        [`FOR i IN 1..10 TAKE WHILE i < 5 OR i < 3 RETURN i`, _.range(1, 5)],
        [`FOR i IN 1..10 TAKE WHILE i < 3 OR i < 5 RETURN i`, _.range(1, 5)],
        [`FOR i IN 1..10 TAKE WHILE ! IS_STRING(i < 5 ? null : "stop") RETURN i`, _.range(1, 5)],
      ];

      for ([query, expected] of cases) {
        const actual = getQueryResults(query);
        assertEqual(expected, actual, `when executing query '${query}'`);
      }
    },
    testTakeUntil: function () {
      const cases = [
        [`FOR i IN 1..10 TAKE UNTIL i >= 5 RETURN i`, _.range(1, 5)],
        [`FOR i IN 1..10 TAKE UNTIL i == 5 RETURN i`, _.range(1, 5)],
        [`FOR i IN 1..10 TAKE UNTIL ! (i != 5) RETURN i`, _.range(1, 5)],
        [`FOR i IN 1..10 TAKE UNTIL NOT (i != 5) RETURN i`, _.range(1, 5)],
        [`FOR i IN 1..10 TAKE UNTIL i >= 5 OR i >= 7 RETURN i`, _.range(1, 5)],
        [`FOR i IN 1..10 TAKE UNTIL i >= 7 OR i >= 5 RETURN i`, _.range(1, 5)],
        [`FOR i IN 1..10 TAKE UNTIL i >= 5 AND i >= 3 RETURN i`, _.range(1, 5)],
        [`FOR i IN 1..10 TAKE UNTIL i >= 3 AND i >= 5 RETURN i`, _.range(1, 5)],
        [`FOR i IN 1..10 TAKE UNTIL ! IS_STRING(i >= 5 ? null : "stop") RETURN i`, _.range(1, 5)],
      ];

      for ([query, expected] of cases) {
        const actual = getQueryResults(query);
        assertEqual(expected, actual, `when executing query '${query}'`);
      }
    },
    testDropWhile: function () {
      const cases = [
        [`FOR i IN 1..10 DROP WHILE i < 5 RETURN i`, _.range(5, 11)],
        [`FOR i IN 1..10 DROP WHILE i != 5 RETURN i`, _.range(5, 11)],
        [`FOR i IN 1..10 DROP WHILE ! (i == 5) RETURN i`, _.range(5, 11)],
        [`FOR i IN 1..10 DROP WHILE NOT (i == 5) RETURN i`, _.range(5, 11)],
        [`FOR i IN 1..10 DROP WHILE i < 5 AND i < 7 RETURN i`, _.range(5, 11)],
        [`FOR i IN 1..10 DROP WHILE i < 7 AND i < 5 RETURN i`, _.range(5, 11)],
        [`FOR i IN 1..10 DROP WHILE i < 5 OR i < 3 RETURN i`, _.range(5, 11)],
        [`FOR i IN 1..10 DROP WHILE i < 3 OR i < 5 RETURN i`, _.range(5, 11)],
        [`FOR i IN 1..10 DROP WHILE ! IS_STRING(i < 5 ? null : "stop") RETURN i`, _.range(5, 11)],
      ];

      for ([query, expected] of cases) {
        const actual = getQueryResults(query);
        assertEqual(expected, actual, `when executing query '${query}'`);
      }
    },
    testDropUntil: function () {
      const cases = [
        [`FOR i IN 1..10 DROP UNTIL i >= 5 RETURN i`, _.range(5, 11)],
        [`FOR i IN 1..10 DROP UNTIL i == 5 RETURN i`, _.range(5, 11)],
        [`FOR i IN 1..10 DROP UNTIL ! (i != 5) RETURN i`, _.range(5, 11)],
        [`FOR i IN 1..10 DROP UNTIL NOT (i != 5) RETURN i`, _.range(5, 11)],
        [`FOR i IN 1..10 DROP UNTIL i >= 5 OR i >= 7 RETURN i`, _.range(5, 11)],
        [`FOR i IN 1..10 DROP UNTIL i >= 7 OR i >= 5 RETURN i`, _.range(5, 11)],
        [`FOR i IN 1..10 DROP UNTIL i >= 5 AND i >= 3 RETURN i`, _.range(5, 11)],
        [`FOR i IN 1..10 DROP UNTIL i >= 3 AND i >= 5 RETURN i`, _.range(5, 11)],
        [`FOR i IN 1..10 DROP UNTIL ! IS_STRING(i >= 5 ? null : "stop") RETURN i`, _.range(5, 11)],
      ];

      for ([query, expected] of cases) {
        const actual = getQueryResults(query);
        assertEqual(expected, actual, `when executing query '${query}'`);
      }
    },
  };
};

const aqlTakeSuite = (whileOrUntil) => function () {
  const negate = whileOrUntil === WHILE ? '' : '!';
  const ucWhileOrUntil = whileOrUntil === WHILE ? 'While' : 'Until';

  return {
    [`testTake${ucWhileOrUntil}WithDocs`]: function () {
      const colName = 'takeCol';
      try {
        const col = db._createDocumentCollection(colName, {numberOfShards: 9});
        col.insert(_.range(1, 200).map(i => ({i})));
        const query = `
          FOR d IN ${colName}
            SORT d.i
            TAKE ${whileOrUntil} (${negate} (d.i < 100))
            COLLECT AGGREGATE lo = MIN(d.i), hi = MAX(d.i), n = COUNT()
            RETURN {lo, hi, n}
        `;
        const expected = {
          lo: 1,
          hi: 99,
          n: 99,
        };
        const actual = getQueryResults(query);
        assertEqual([expected], actual);
      } catch (e) {
        db._drop(colName);
        throw e;
      }
      db._drop(colName);
    },
  };
};

const aqlDropSuite = (whileOrUntil) => function () {
  const negate = whileOrUntil === WHILE ? '' : '!';
  const ucWhileOrUntil = whileOrUntil === WHILE ? 'While' : 'Until';

  return {
    [`testDrop${ucWhileOrUntil}Simple`]: function () {
      const colName = 'takeCol';
      try {
        const col = db._createDocumentCollection(colName, {numberOfShards: 9});
        col.insert(_.range(1, 200).map(i => ({i})));
        const query = `
          FOR d IN ${colName}
            SORT d.i
            DROP ${whileOrUntil} (${negate} (d.i < 100))
            COLLECT AGGREGATE lo = MIN(d.i), hi = MAX(d.i), n = COUNT()
            RETURN {lo, hi, n}
        `;
        const expected = {
          lo: 100,
          hi: 199,
          n: 100,
        };
        const actual = getQueryResults(query);
        assertEqual([expected], actual);
      } catch (e) {
        db._drop(colName);
        throw e;
      }
      db._drop(colName);
    },
  };
};

jsunity.run(aqlTakeDropWhileUntilSyntaxSuite);
jsunity.run(aqlTakeSuite(WHILE));
jsunity.run(aqlTakeSuite(UNTIL));
jsunity.run(aqlDropSuite(WHILE));
jsunity.run(aqlDropSuite(UNTIL));

return jsunity.done();

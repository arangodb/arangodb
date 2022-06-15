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

const WHILE = 'WHILE';
const UNTIL = 'UNTIL';

const aqlTakeSuite = (whileOrUntil) => function () {
  const negate = whileOrUntil === WHILE ? '' : '!';
  const ucWhileOrUntil = whileOrUntil === WHILE ? 'While' : 'Until';

  return {
    [`testTake${ucWhileOrUntil}Simple`]: function () {
      const query = `
        FOR i IN 1..1000
          TAKE ${whileOrUntil} (${negate} (i < 5))
          RETURN i
      `;
      const expected = _.range(1, 5);
      const actual = getQueryResults(query);
      assertEqual(expected, actual);
    },
  };
};

const aqlDropSuite = (whileOrUntil) => function () {
  const negate = whileOrUntil === WHILE ? '' : '!';
  const ucWhileOrUntil = whileOrUntil === WHILE ? 'While' : 'Until';

  return {
    [`testDrop${ucWhileOrUntil}Simple`]: function () {
      const query = `
        FOR i IN 1..1000
          TAKE ${whileOrUntil} (${negate} (i < 995))
          RETURN i
      `;
      const expected = _.range(995, 1001);
      const actual = getQueryResults(query);
      assertEqual(expected, actual);
    },
  };
};

jsunity.run(aqlTakeSuite(WHILE));
jsunity.run(aqlTakeSuite(UNTIL));
// jsunity.run(aqlDropSuite(WHILE));
// jsunity.run(aqlDropSuite(UNTIL));

return jsunity.done();
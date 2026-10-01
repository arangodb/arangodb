/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// //////////////////////////////////////////////////////////////////////////////

const maxRuntime = 2.0;
// Aql analyzers take their execution budget from --query.max-runtime.
// Keep this value equal to the lower/upper bounds in the suite below.
if (getOptions === true) {
  return {
    'query.max-runtime': maxRuntime
  };
}

const jsunity = require('jsunity');
const db = require('internal').db;
const analyzers = require('@arangodb/analyzers');
const time = require('internal').time;
const internal = require('internal');

const unboundedQuery = 'FOR i IN 1..1000000000 FOR j IN 1..1000000000 return 1';

function aqlAnalyzerMaxRuntimeTestSuite() {
  const hang = 'AqlAnalyzerRuntimeHang';
  const harmless = 'AqlAnalyzerHarmless';

  function tokens(name, value) {
    const result = db._query('return TOKENS(@value, @name)', {
      value: value,
      name: name
    }).toArray()[0];
    return result;
  }

  return {
    setUpAll: function () {
      analyzers.save(harmless, 'aql', {queryString: 'return UPPER(@param)'});
      analyzers.save(hang, 'aql', {queryString: unboundedQuery});
    },

    tearDownAll: function () {
      try { analyzers.remove(hang, true); } catch (err) {}
      try { analyzers.remove(harmless, true); } catch (err) {}
    },

    testHarmlessAnalyzerStillRuns: function () {
      const result = tokens(harmless, 'x');
      assertEqual(['X'], result);
    },

    testUnboundedAnalyzerIsInterrupted: function () {
      let exceptionThrown = false;
      let result;
      let started = 0.0;
      let ended = 0.0;
      try {
        started = time();
        result = tokens(hang, 'x');
      } catch (e) {
        exceptionThrown = true;

        ended = time();
        const elapsed = ended - started;
        assertTrue(elapsed >= maxRuntime && elapsed < (1.1 * maxRuntime));

        assertEqual(e.error, true);
        assertEqual(e.errorMessage, "AQL: query killed (while executing)");
      }

      assertEqual(result, undefined, "`result` should not contain a value");
      assertEqual(exceptionThrown, true, "Query should have been killed and exception thrown");
    }
  };
}

jsunity.run(aqlAnalyzerMaxRuntimeTestSuite);
return jsunity.done();

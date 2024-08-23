/*jshint globalstrict:false, strict:false */
/* global getOptions, assertNotEqual, assertMatch, assertNotMatch, arango */

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
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.ensure-whitespace-metrics-format': "true",
  };
}

const jsunity = require('jsunity');

function testSuite() {
  let getMetrics = function() {
    let res = arango.GET_RAW(`/_admin/metrics/v2`);
    return res.body.split(/\n/).filter((l) => l.trim() !== '').map((l) => l.trim());
  };

  return {
    testMetricsContainWhitespace : function() {
      let lines = getMetrics();
      assertNotEqual(0, lines);

      lines.forEach((l) => {
        // ignore TYPE and HELP
        if (l.match(/^#/)) {
          return;
        }
        assertNotMatch(/^.*[a-zA-Z\}]([0-9]+(\.[0-9]+)?)$/, l);
        // whitespace is _required_ in this setup
        assertMatch(/^.*[0-9a-zA-Z\}]\s+([0-9]+(\.[0-9]+)?)$/, l);
      });
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();

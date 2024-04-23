/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, arango, assertEqual */

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
    'log.max-queued-entries': '0',
    'javascript.allow-admin-execute': 'true',
    'log.force-direct': 'false',
    'log.foreground-tty': 'false',
  };
}

const jsunity = require('jsunity');
const getMetric = require('@arangodb/test-helper').getMetricSingle;

function LoggerSuite() {
  'use strict';

  return {
    testDroppedMessages: function () {
      const oldValue = getMetric("arangodb_logger_messages_dropped_total");

      let res = arango.POST_RAW("/_admin/execute", "for (let i = 0; i < 100; ++i) { require('console').error('abc'); }");
      assertEqual(200, res.code, res);

      const newValue = getMetric("arangodb_logger_messages_dropped_total");
      assertTrue(newValue >= oldValue + 100, {oldValue, newValue});
    },

  };
}

jsunity.run(LoggerSuite);
return jsunity.done();

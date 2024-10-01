/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, arango, assertMatch, assertEqual */

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

const fs = require('fs');

if (getOptions === true) {
  return {
    'log.output': 'file://' + fs.getTempFile() + '.$PID',
    'log.force-direct': 'true',
    'rocksdb.wal-archive-size-limit': '10000000', // roughly 10MB
    'rocksdb.wal-file-timeout-initial': '10',
  };
}

const jsunity = require('jsunity');
const { deriveTestSuite } = require('@arangodb/test-helper');
const { WalArchiveSizeLimitSuite, sendLargeServerOperation } = require(fs.join('tests', 'js', 'client', 'server_parameters', 'wal-archive-size-limit.inc'));

function WalArchiveSizeLimitSuiteSmall() {
  'use strict';
      
  let suite = {
    testDoesNotForceDeleteWalFiles: function() {
      let res = sendLargeServerOperation(300);

      // this will fail if warning d9793 was *not* logged
      let found = false;
      assertTrue(res[0].match(/testmann: start/));
      for (let i = 0; i < res.length; ++i) {
        if (res[i].match(/d9793/)) {
          found = true;
          break;
        }
      }

      assertTrue(found);
    },
  };

  deriveTestSuite(WalArchiveSizeLimitSuite(), suite, '_small');
  return suite;
}

jsunity.run(WalArchiveSizeLimitSuiteSmall);
return jsunity.done();

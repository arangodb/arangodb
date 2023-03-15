/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, arango, assertMatch, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for server startup options
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const fs = require('fs');

if (getOptions === true) {
  return {
    'log.output': 'file://' + fs.getTempFile() + '.$PID',
    'log.force-direct': 'true',
    'rocksdb.wal-archive-size-limit': '32000000000', // roughly 32GB
    'rocksdb.wal-file-timeout-initial': '10',
  };
}

const jsunity = require('jsunity');
const { deriveTestSuite } = require('@arangodb/test-helper');
const { WalArchiveSizeLimitSuite, sendLargeServerOperation } = require(fs.join('tests', 'js', 'client', 'server_parameters', 'wal-archive-size-limit.inc'));

function WalArchiveSizeLimitSuiteLarge() {
  'use strict';

  let suite = {
    testDoesNotForceDeleteWalFiles: function() {
      let res = sendLargeServerOperation(20);

      // this will fail if warning d9793 was logged
      assertEqual(2, res.length);
          
      assertTrue(res[0].match(/testmann: start/));
      assertTrue(res[1].match(/testmann: done/));
    },
  };

  deriveTestSuite(WalArchiveSizeLimitSuite(), suite, '_large');
  return suite;
}

jsunity.run(WalArchiveSizeLimitSuiteLarge);
return jsunity.done();

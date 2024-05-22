/*jshint globalstrict:false, strict:false */
/* global arango, getOptions, runSetup, assertTrue, assertEqual */

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

const helper = require('@arangodb/testutils/block-cache-test-helper');
const cn = 'UnitTestsCollection';

if (getOptions === true) {
  return {
    'foxx.queues' : 'false',
    'server.statistics' : 'false',
    runSetup: true,
  };
}
if (runSetup === true) {
  helper.setup(cn);
  return true;
}

const jsunity = require('jsunity');

function FillBlockCacheSuite() {
  'use strict';

  return {
    
    testHttpApi: function() {
      helper.testHttpApi(cn, true);
    },

  };
}

jsunity.run(FillBlockCacheSuite);
return jsunity.done();

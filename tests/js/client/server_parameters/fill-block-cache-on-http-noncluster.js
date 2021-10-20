/*jshint globalstrict:false, strict:false */
/* global arango, getOptions, runSetup, assertTrue, assertEqual */

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

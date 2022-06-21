/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertTrue, assertFalse, assertEqual, arango, require*/

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const _ = require("lodash");

function headersSingleSuite () {
  'use strict';
  
  return {
    testSingle: function() {
      let result = arango.GET_RAW("/_api/version");
      assertTrue(result.hasOwnProperty("headers"));
      if (arango.protocol() !== 'vst') {
        // VST does not send this header, never
        assertTrue(result.headers.hasOwnProperty("server"), result);
        assertEqual("ArangoDB", result.headers["server"]);
      }
    },
  };
}

jsunity.run(headersSingleSuite);
return jsunity.done();

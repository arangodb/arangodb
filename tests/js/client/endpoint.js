/*jshint globalstrict:false, strict:false, unused: false */
/*global assertEqual, assertTrue, arango  */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Spec for foxx manager
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Michael Hackstein
// / @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////
var jsunity = require("jsunity");

const time = require('internal').time;
const ArangoError = require('@arangodb').ArangoError;
const errors = require('internal').errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function EndpointConnectionSuite() {
  return {
    testVersionCallWorks: function() {
      let ver;
      try {
        ver = db._version();
      } catch (x) {
        print(x);
      }
      assertTrue(ver != undefined);
    },
    testForceJson: function() {
      if (require('internal').options()['server.force-json']) {
        let req = arango.GET_RAW('/_api/version');
        assertNotEqual(req.headers['content-type'], 'application/x-velocypack');
      }
    },
    testTimeout: function() {
      let testCode = `require("internal").sleep(25)`;
      
      let before = time();
      res = arango.POST_RAW('/_admin/execute?returnAsJSON=true',
                            testCode);
      let delta = time() - before;
      assertTrue(res.error);
      assertEqual(res.errorNum, 503);
      assertEqual(res.errorMessage, "Request timeout");
      // Server timeout is configured for 2 seconds:
      print(delta);
      assertTrue(delta < 3, `connection timeout should be around 2s - it actually was ${delta}`);
      assertTrue(delta >= 2, `connection timeout should be around 2s - it actually was ${delta}`);

      before = time();
      res = arango.POST_RAW('/_admin/execute?returnAsJSON=true',
                            testCode,
                            {'Keep-Alive': 5});
      delta = time() - before;
      assertTrue(res.error);
      assertEqual(res.errorNum, 503);
      assertEqual(res.errorMessage, "Request timeout");
      // Keep-Alive header should superseede server settings:
      print(delta);
      assertTrue(delta >= 5, `connection timeout should be around 5s - it actually was ${delta}`);
      assertTrue(delta < 6, `connection timeout should be around 5s - it actually was ${delta}`);

    }
  };   
};


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(EndpointConnectionSuite);

return jsunity.done();

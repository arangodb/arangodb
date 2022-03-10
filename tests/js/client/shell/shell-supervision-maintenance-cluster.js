/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertFalse, assertMatch, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for supervision maintenance API
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');

function numberOfServersSuite () {
  const url = "/_admin/cluster/maintenance";
  const re = /(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)/;

  return {
    tearDown : function () {
      // turn off supervision maintenance mode again
      arango.PUT(url, '"off"');
    },

    testGetSupervisionDisabled : function () {
      let result = arango.GET(url);
      assertFalse(result.error);
      assertEqual("Normal", result.result);
    },
    
    testGetSupervisionEnabled : function () {
      // default maintenance period is 1 hour
      let result = arango.PUT(url, '"on"');
      assertFalse(result.error);

      result = arango.GET(url);
      assertFalse(result.error);
      assertEqual("Maintenance", result.result);
    },
    
    testSetSupervisionEnabledWrongData : function () {
      let result = arango.PUT(url, '"piff!"');
      assertTrue(result.error);
      assertEqual(400, result.code);
      
      result = arango.GET(url);
      assertFalse(result.error);
      assertEqual("Normal", result.result);
    },
    
    testSetSupervisionEnabledDefaultTimeout : function () {
      const now = Date.now();
      // default maintenance period is 1 hour
      let result = arango.PUT(url, '"on"');
      assertFalse(result.error);
      // we expect the reactivation date to be returned
      assertMatch(/\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z/, result.warning);

      let match = result.warning.match(re);
      assertTrue(Array.isArray(match));

      let reactivationTime = match[0];
      assertMatch(re, reactivationTime);

      const reactivate = Date.parse(reactivationTime);

      // difference between dates should be at least one hour
      // we subtract one second because the returned datetime string does not
      // have millisecond precision
      assertTrue(reactivate - now >= (3600 * 1000) - 1000, { now, reactivate });

      result = arango.GET(url);
      assertFalse(result.error);
      assertEqual("Maintenance", result.result);
    },
    
    testSetSupervisionEnabled30MinutesTimeout : function () {
      const now = Date.now();
      let result = arango.PUT(url, '"1800"');
      assertFalse(result.error);
      // we expect the reactivation date to be returned
      assertMatch(/\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z/, result.warning);

      let match = result.warning.match(re);
      assertTrue(Array.isArray(match));

      let reactivationTime = match[0];
      assertMatch(re, reactivationTime);

      const reactivate = Date.parse(reactivationTime);

      // we subtract one second because the returned datetime string does not
      // have millisecond precision
      assertTrue(reactivate - now >= (1800 * 1000) - 1000, { now, reactivate });

      result = arango.GET(url);
      assertFalse(result.error);
      assertEqual("Maintenance", result.result);
    },
    
    testSetSupervisionEnabled5HoursTimeout : function () {
      const now = Date.now();
      let result = arango.PUT(url, '"18000"');
      assertFalse(result.error);
      // we expect the reactivation date to be returned
      assertMatch(/\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z/, result.warning);

      let match = result.warning.match(re);
      assertTrue(Array.isArray(match));

      let reactivationTime = match[0];
      assertMatch(re, reactivationTime);

      const reactivate = Date.parse(reactivationTime);
      
      // we subtract one second because the returned datetime string does not
      // have millisecond precision
      assertTrue(reactivate - now >= (18000 * 1000) - 1000, { now, reactivate });

      result = arango.GET(url);
      assertFalse(result.error);
      assertEqual("Maintenance", result.result);
    },
    
    testSetSupervisionEnabledNumericTimeout : function () {
      const now = Date.now();
      let result = arango.PUT(url, '666');
      assertFalse(result.error);
      // we expect the reactivation date to be returned
      assertMatch(/\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z/, result.warning);

      let match = result.warning.match(re);
      assertTrue(Array.isArray(match));

      let reactivationTime = match[0];
      assertMatch(re, reactivationTime);

      const reactivate = Date.parse(reactivationTime);

      // we subtract one second because the returned datetime string does not
      // have millisecond precision
      assertTrue(reactivate - now >= (666 * 1000) - 1000, { now, reactivate });

      result = arango.GET(url);
      assertFalse(result.error);
      assertEqual("Maintenance", result.result);
    },

  };
}

jsunity.run(numberOfServersSuite);
return jsunity.done();

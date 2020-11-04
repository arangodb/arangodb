/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

let jsunity = require("jsunity");
let arangodb = require("@arangodb");
let internal = require("internal");
let request = require("@arangodb/request");
let db = arangodb.db;

function getEndpointById(id) {
  const toEndpoint = (d) => (d.endpoint);
  const endpointToURL = (endpoint) => {
    if (endpoint.substr(0, 6) === 'ssl://') {
      return 'https://' + endpoint.substr(6);
    }
    let pos = endpoint.indexOf('://');
    if (pos === -1) {
      return 'http://' + endpoint;
    }
    return 'http' + endpoint.substr(pos);
  };

  const instanceInfo = JSON.parse(internal.env.INSTANCEINFO);
  return instanceInfo.arangods.filter((d) => (d.id === id))
                              .map(toEndpoint)
                              .map(endpointToURL)[0];
}

function getEndpointsByType(type) {
  const isType = (d) => (d.role.toLowerCase() === type);
  const toEndpoint = (d) => (d.endpoint);
  const endpointToURL = (endpoint) => {
    if (endpoint.substr(0, 6) === 'ssl://') {
      return 'https://' + endpoint.substr(6);
    }
    let pos = endpoint.indexOf('://');
    if (pos === -1) {
      return 'http://' + endpoint;
    }
    return 'http' + endpoint.substr(pos);
  };

  const instanceInfo = JSON.parse(internal.env.INSTANCEINFO);
  return instanceInfo.arangods.filter(isType)
                              .map(toEndpoint)
                              .map(endpointToURL);
}

/// @brief set failure point
function debugCanUseFailAt(endpoint) {
  let res = request.get({
    url: endpoint + '/_admin/debug/failat',
  });
  return res.status === 200;
}

/// @brief set failure point
function debugSetFailAt(endpoint, failAt) {
  let res = request.put({
    url: endpoint + '/_admin/debug/failat/' + failAt,
    body: ""
  });
  if (res.status !== 200) {
    throw "Error setting failure point";
  }
}

function debugClearFailAt(endpoint) {
  let res = request.delete({
    url: endpoint + '/_admin/debug/failat',
    body: ""
  });
  if (res.status !== 200) {
    throw "Error removing failure points";
  }
}

function RecalculateCountSuite() {
  const cn = "UnitTestsCollection";

  return {

    setUp : function () {
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      db._drop(cn);
    },

    tearDown : function () {
      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
      db._drop(cn);
    },
    
    testFixBrokenCounts : function () {
      let c = db._create(cn, { numberOfShards: 5 });

      getEndpointsByType("dbserver").forEach((ep) => debugSetFailAt(ep, "DisableCommitCounts"));

      for (let i = 0; i < 1000; ++i) {
        c.insert({});
      }

      assertNotEqual(1000, c.count());
      assertEqual(1000, c.toArray().length);

      getEndpointsByType("dbserver").forEach((ep) => debugClearFailAt(ep));
 
      c.recalculateCount();
      
      assertEqual(1000, c.count());
      assertEqual(1000, c.toArray().length);
    },
    
  };
}

let ep = getEndpointsByType('dbserver');
if (ep.length && debugCanUseFailAt(ep[0])) {
  jsunity.run(RecalculateCountSuite);
}
return jsunity.done();

/* jshint strict: false, sub: true */
/* global print, arango */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// / Copyright 2014 triagens GmbH, Cologne, Germany
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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const fs = require('fs');

const download = internal.download;
const time = internal.time;
const sleep = internal.sleep;

const pu = require('@arangodb/testutils/process-utils');

const instanceInfo = JSON.parse(internal.env.INSTANCEINFO);
const options = JSON.parse(internal.env.OPTIONS);
const outfn = fs.join(instanceInfo.rootDir, 'stats.jsonl');
const opts = Object.assign(pu.makeAuthorizationHeaders(options),
                           { method: 'GET' });

while(true) {
  let state = {
    state: true,
    before: time(),
    delta: [],
    fails: []
  };
  let results = [];
  for (let i = 0; i < 60; i++) {
  const before = time();
    let oneSet = { state: true };
    results.push(oneSet);
    instanceInfo.arangods.forEach(arangod => {
      let serverId = arangod.role + '_' + arangod.port;
      let beforeCall = time();
      let procStats = pu.getProcessStats(arangod.pid);
      if (arangod.role === "agent") {
        let reply = download(arangod.url + '/_api/version', '', opts);
        if (reply.hasOwnProperty('error') || reply.code !== 200) {
          print("fail: " + JSON.stringify(reply) +
                " - ps before: " + JSON.stringify(procStats) +
                " - ps now: " + JSON.stringify(pu.getProcessStats(arangod.pid)));
          state.state = false;
          oneSet.state = false;
          oneSet[serverId] = {
            error: true,
            start: beforeCall,
            delta: time() - beforeCall
          };
        } else {
          let statisticsReply = JSON.parse(reply.body);
          oneSet[serverId] = {
            error: false,
            start: beforeCall,
            delta: time() - beforeCall
          };
        }
      } else {
        let reply = download(arangod.url + '/_admin/statistics', '', opts);
        if (reply.hasOwnProperty('error') || reply.code !== 200) {
          print("fail: " + JSON.stringify(reply) +
                " - ps before: " + JSON.stringify(procStats) +
                " - ps now: " + JSON.stringify(pu.getProcessStats(arangod.pid)));
          state.state = false;
          oneSet.state = false;
          oneSet[serverId] = {
            error: true,
            start: beforeCall,
            delta: time() - beforeCall
          };
        } else {
          let statisticsReply = JSON.parse(reply.body);
          oneSet[serverId] = {
            error: false,
            start: beforeCall,
            delta: time() - beforeCall,
            uptime: statisticsReply.server.uptime
          };
        }
      }
    });
    state['delta'].push(time() - before);
    if (state.delta > 1000) {
      print("marking FAIL since it took to long");
      state.state = false;
    }
    if (!oneSet.state) {
      state.fails.push(oneSet);
    }
    sleep(1);
  }
  fs.append(outfn, JSON.stringify(state) + "\n");

}

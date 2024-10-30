/* jshint strict: false, sub: true */
/* global print, arango */
'use strict';

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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

const crypto = require('@arangodb/crypto');
const fs = require('fs');
const tu = require('@arangodb/testutils/test-utils');
const pu = require('@arangodb/testutils/process-utils');
const jsunity = require('jsunity');
const {assertTrue, assertFalse, assertEqual} = jsunity.jsUnity.assertions;
const arangosh = require('@arangodb/arangosh');
const request = require('@arangodb/request');
const isArm = require("@arangodb/test-helper-common").versionHas('arm');

const internal = require('internal');
const {
  arango,
  db,
  download,
  time,
  sleep
} = internal;
// const BLUE = internal.COLORS.COLOR_BLUE;
const CYAN = internal.COLORS.COLOR_CYAN;
const GREEN = internal.COLORS.COLOR_GREEN;
const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;
// const YELLOW = internal.COLORS.COLOR_YELLOW;
const seconds = x => x * 1000;

class agencyMgr {
  constructor(options, wholeCluster) {
    this.options = options;
    this.wholeCluster = wholeCluster;
    this.agencySize = options.agencySize;
    this.supervision = String(options.agencySupervision);
    this.waitForSync = false;
    if (options.agencyWaitForSync !== undefined) {
      this.waitForSync = options.agencyWaitForSync;
    }
    this.agencyInstances = [];
    this.agencyEndpoint = "";
    this.agentsLaunched = 0;
    this.urls = [];
    this.endpoints = [];
    this.leader = null;
    this.dumpedAgency = false;
  }
  getStructure() {
    return {
      agencySize: this.agencySize,
      agencyInstances: this.agencyInstances.length,
      supervision: this.supervision,
      waitForSync: this.waitForSync,
      agencyEndpoint: this.agencyEndpoint,
      agentsLaunched: this.agentsLaunched,
      urls: this.urls,
      endpoints: this.endpoints
    };
  }
  setFromStructure(struct) {
    this.agencySize = struct['agencySize'];
    this.supervision = struct['supervision'];
    this.waitForSync = struct['waitForSync'];
    this.agencyEndpoint = struct['agencyEndpoint'];
    this.agentsLaunched = struct['agentsLaunched'];
    this.urls = struct['urls'];
    this.endpoints = struct['endpoints'];
  }
  getAgency(path, method, body = null) {
    return this.getAnyAgent(this.agencyInstances[0], path, method, body);
  }

  shouldBeCompleted() {
    let count = 0;
    this.agencyInstances.forEach(agent => { if (agent.pid !== null) {count ++;}});
    return count === this.agencySize;
  }

  moreIsAlreadyRunning() {
    let count = 0;
    this.wholeCluster.arangods.forEach(arangod => { if (arangod.pid !== null) {count ++;}});
    return count > this.agencySize;
  }
  getAnyAgent(agent, path, method, body = null) {
    while(true) {
      let opts = {};
      if (body === null) {
        body = (method === 'POST') ? '[["/"]]' : '';
      }
      if (agent.JWT) {
        opts = pu.makeAuthorizationHeaders(agent.options, agent.args, agent.JWT);
      } else {
        let allArgs = [agent.args, agent.moreArgs];
        allArgs.forEach(args => {
          if (allArgs.hasOwnProperty('authOpts')) {
            opts['jwt'] = crypto.jwtEncode(agent.authOpts['server.jwt-secret'], {'server_id': 'none', 'iss': 'arangodb'}, 'HS256');
          } else if (allArgs.hasOwnProperty('server.jwt-secret')) {
            opts['jwt'] = crypto.jwtEncode(args['server.jwt-secret'], {'server_id': 'none', 'iss': 'arangodb'}, 'HS256');
          } else if (agent.jwtFiles) {
            opts['jwt'] = crypto.jwtEncode(fs.read(agent.jwtFiles[0]), {'server_id': 'none', 'iss': 'arangodb'}, 'HS256');
          }
        });
      }
      opts['method'] = method;
      opts['returnBodyOnError'] = true;
      opts['followRedirects'] = false;
      let ret = download(agent.url + path, body, opts);
      if (ret.code !== 307 && ret.code !== 303) {
        return ret;
      }
      try {
        let newAgentUrl = ret.headers['location'];
        agent = this.agencyInstances.filter(agent => { return newAgentUrl.search(agent.url) === 0;})[0];
      } catch (ex) {
        throw new Error(`couldn't find agent to redirect to ${JSON.stringify(ret)}`);
      }
      print(`following redirect to ${agent.name}`);
    }
  }
  postAgency(operation, body = null) {
    let res = this.getAnyAgent(this.agencyInstances[0],
                               `/_api/agency/${operation}`,
                               'POST',
                               JSON.stringify(body));
    assertTrue(res.hasOwnProperty('code'), JSON.stringify(res));
    assertEqual(res.code, 200, JSON.stringify(res));
    assertTrue(res.hasOwnProperty('message'));
    return arangosh.checkRequestResult(JSON.parse(res.body));
  }
  call(operation, body) {
    return this.postAgency(operation, body);
  }

  get(key) {
    const res = this.postAgency( 'read', [[
      `/arango/${key}`,
    ]]);
    return res[0];
  }
  set (path, value) {
    return this.postAgency('write', [[{
      [`/arango/${path}`]: {
        'op': 'set',
        'new': value,
      },
    }]]);
  }
  remove(path) {
    return this.postAgency('write', [[{
      [`/arango/${path}`]: {
        'op': 'delete'
      },
    }]]);
  }
  transact(body) {
    return this.postAgency("transact", body);
  }
  increaseVersion(path) {
    return this.postAgency('write', [[{
      [`/arango/${path}`]: {
        'op': 'increment',
      },
    }]]);
  }

  dumpAgent(agent, path, method, fn, dumpdir) {
    print('--------------------------------- '+ fn + ' -----------------------------------------------');
    let agencyReply = this.getAnyAgent(agent, path, method);
    if (agencyReply.code === 200) {
      if (fn === "agencyState") {
        fs.write(fs.join(dumpdir, `${fn}_${agent.pid}.json`), agencyReply.body);
      } else {
        let agencyValue = JSON.parse(agencyReply.body);
        fs.write(fs.join(dumpdir, `${fn}_${agent.pid}.json`), JSON.stringify(agencyValue, null, 2));
      }
    } else {
      print(agencyReply);
    }
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief dump the state of the agency to disk. if we still can get one.
  // //////////////////////////////////////////////////////////////////////////////
  dumpAgency() {
    if ((this.options.agency === false) || (this.options.dumpAgencyOnError === false) || this.dumpedAgency) {
      return;
    }

    this.dumpedAgency = true;
    const dumpdir = fs.join(this.options.testOutputDirectory, `agencydump_${this.instanceCount}`);
    const zipfn = fs.join(this.options.testOutputDirectory, `agencydump_${this.instanceCount}.zip`);
    if (fs.isFile(zipfn)) {
      fs.remove(zipfn);
    };
    if (fs.exists(dumpdir)) {
      fs.list(dumpdir).forEach(file => {
        const fn = fs.join(dumpdir, file);
        if (fs.isFile(fn)) {
          fs.remove(fn);
        }
      });
    } else {
      fs.makeDirectory(dumpdir);
    }
    this.agencyInstances.forEach(arangod => {
      if (!arangod.checkArangoAlive()) {
        print(Date() + " this agent is already dead: " + JSON.stringify(arangod.getStructure()));
      } else {
        print(Date() + " Attempting to dump Agent: " + JSON.stringify(arangod.getStructure()));
        this.dumpAgent(arangod,  '/_api/agency/config', 'GET', 'agencyConfig', dumpdir);

        this.dumpAgent(arangod, '/_api/agency/state', 'GET', 'agencyState', dumpdir);

        this.dumpAgent(arangod, '/_api/agency/read', 'POST', 'agencyPlan', dumpdir);
      }
    });
    let zipfiles = [];
    fs.list(dumpdir).forEach(file => {
      const fn = fs.join(dumpdir, file);
      if (fs.isFile(fn)) {
        zipfiles.push(file);
      }
    });
    print(`${CYAN}${Date()} Zipping ${zipfn}${RESET}`);
    fs.zipFile(zipfn, dumpdir, zipfiles);
    zipfiles.forEach(file => {
      fs.remove(fs.join(dumpdir, file));
    });
    fs.removeDirectory(dumpdir);
  }
  getFromPlan(path) {
    let req = this.getAnyAgent(this.agencyInstances[0], '/_api/agency/read', 'POST', `[["/arango/${path}"]]`);
    if (req.code !== 200) {
      throw new Error(`Failed to query agency [["/arango/${path}"]] : ${JSON.stringify(req)}`);
    }
    return JSON.parse(req["body"])[0];
  }

  removeServerFromAgency(serverId) {
    // Make sure we remove the server
    for (let i = 0; i < 10; ++i) {
      const res = arango.POST_RAW("/_admin/cluster/removeServer", JSON.stringify(serverId));
      if (res.code === 404 || res.code === 200) {
        // Server is removed
        return;
      }
      // Server could not be removed, give supervision some more time
      // and then try again.
      print("Wait for supervision to clear responsibilty of server");
      require("internal").wait(0.2);
    }
    // If we reach this place the server could not be removed
    // it is still responsible for shards, so a failover
    // did not work out.
    throw "Could not remove shutdown server";
  }

  dumpAgencyIfNotYet(forceTerminate, timeoutReached) {
    if (forceTerminate && !timeoutReached && !this.dumpedAgency) {
      // the SUT is unstable, but we want to attempt an agency dump anyways.
      try {
        print(CYAN + Date() + ' attempting to dump agency in forceterminate!' + RESET);
        // set us a short deadline so we don't get stuck.
        internal.SetGlobalExecutionDeadlineTo(this.options.oneTestTimeout * 100);
        this.dumpAgency();
      } catch(ex) {
        print(CYAN + Date() + ' aborted to dump agency in forceterminate! ' + ex + RESET);
      } finally {
        internal.SetGlobalExecutionDeadlineTo(0.0);
      }
    }
  }
  
  detectAgencyAlive(httpAuthOptions) {
    if (!this.options.agency ||
        !this.shouldBeCompleted() ||
        this.moreIsAlreadyRunning()) {
      print("no agency check this time.");
      return;
    }
    let count = (isArm || this.options.isInstrumented) ? 75 : 25;
    while (count > 0) {
      let haveConfig = 0;
      let haveLeader = 0;
      let leaderId = null;
      for (let agentIndex = 0; agentIndex < this.agencySize; agentIndex ++) {
        let reply = this.getAnyAgent(this.agencyInstances[agentIndex], '/_api/agency/config', 'GET');
        if (this.options.extremeVerbosity) {
          print("Response ====> ");
          print(reply);
        }
        if (!reply.error && reply.code === 200) {
          let res = JSON.parse(reply.body);
          if (res.hasOwnProperty('lastAcked')){
            haveLeader += 1;
          }
          if (res.hasOwnProperty('leaderId') && res.leaderId !== "") {
            haveConfig += 1;
            if (leaderId === null) {
              leaderId = res.leaderId;
            } else if (leaderId !== res.leaderId) {
              haveLeader = 0;
              haveConfig = 0;
            }
          }
        }
        if (haveLeader === 1 && haveConfig === this.agencySize) {
          print("Agency Up!");
          try {
            // set back log level to info for agents
            for (let agentIndex = 0; agentIndex < this.agencySize; agentIndex ++) {
              this.getAnyAgent(this.agencyInstances[agentIndex], '/_admin/log/level', 'PUT', JSON.stringify({"agency":"info"}));
            }
          } catch (err) {}
          return;
        }
        if (count === 0) {
          throw new Error("Agency didn't come alive in time!");
        }
        sleep(0.5);
        count --;
      }
    }
  }
}

exports.registerOptions = function(optionsDefaults, optionsDocumentation, optionHandlers) {
  tu.CopyIntoObject(optionsDefaults, {
    'dumpAgencyOnError': true,
    'agencySize': 3,
    'agencyWaitForSync': false,
    'agencySupervision': true,
  });

  tu.CopyIntoList(optionsDocumentation, [
    ' SUT agency configuration properties:',
    '   - `agency`: if set to true agency tests are done',
    '   - `agencySize`: number of agents in agency',
    '   - `agencySupervision`: run supervision in agency',
    '   - `dumpAgencyOnError`: if we should create an agency dump if an error occurs',
    ''
  ]);
};
exports.agencyMgr = agencyMgr;

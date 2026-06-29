/*global describe, it, ArangoAgency, after, afterEach, instanceManager, fail */

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
/// @author Andreas Streichardt
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const expect = require('chai').expect;

const arango = require("@arangodb").arango;
const internal = require('internal');
const db = internal.db;
const heartbeatInterval = 1; // 1 second
let isCluster = instanceManager.arangods.length > 1;
let download = require('internal').download;
let endpoint = instanceManager.url;

const waitForHeartbeat = function () {
  internal.wait(3 * heartbeatInterval, false);
};

const setReadOnlyAndGetDBServer = function () {
  let res = arango.GET('/_admin/cluster/health');
  let servers = Object.keys(res.Health).filter(function (s) {
    return s.match(/^PRMR/);
  });

  let resp = arango.PUT('/_admin/server/mode', { mode: 'readonly' });
  expect(resp.code).to.equal(200);
  waitForHeartbeat();

  return servers[0];
};

// this only tests the http api...there is a separate readonly test
describe('Readonly mode api', function () {
  afterEach(function () {
    // restore default server mode
    arango.PUT('/_admin/server/mode', { mode: 'default' });
    waitForHeartbeat();
  });

  after(function () {
    // wait for heartbeats so the "default" server mode has a chance to be picked up by all db servers
    // before we go on with other tests
    waitForHeartbeat();
  });

  it('outputs its current mode', function () {
    let resp = arango.GET('/_admin/server/mode');
    expect(resp.code).to.equal(200);
    expect(resp).to.have.property('mode', 'default');
  });

  it('can switch to readonly', function () {
    let resp = arango.PUT('/_admin/server/mode', { mode: 'readonly' });
    expect(resp.code).to.equal(200);
    waitForHeartbeat();
    expect(resp).to.have.property('mode', 'readonly');
  });

  it('throws an error when not passing an object', function () {
    let set = arango.PUT('/_admin/server/mode', 'readonly');
    expect(set.code).to.equal(400);
    waitForHeartbeat();

    let resp = arango.GET('/_admin/server/mode');
    expect(resp).to.have.property('mode', 'default');
  });

  it('throws an error when passing an unknown mode', function () {
    let set = arango.PUT('/_admin/server/mode', { mode: 'testi' });
    expect(set.code).to.equal(400);
    waitForHeartbeat();

    let resp = arango.GET('/_admin/server/mode');
    expect(resp).to.have.property('mode', 'default');
  });

  it('the heartbeat should set readonly mode for all cluster nodes', function () {
    let resp = arango.PUT('/_admin/server/mode', { mode: 'readonly' });
    expect(resp.code).to.equal(200);
    waitForHeartbeat();

    let res = instanceManager.arangods.filter(arangod => arangod.role === 'single' || arangod.role === 'coordinator' || arangod.role === 'primary')
      .every(arangod => {
        //Left as a download() because it does not execute.
        let resp = download(arangod.url + '/_admin/server/mode');
        if (resp.code === 503) {
          // called on a follower
          expect(resp.headers).to.have.property('x-arango-endpoint');
        } else {
          let body = JSON.parse(resp.body);
          expect(body).to.have.property('mode', 'readonly');
        }
      });
  });

  it('can still access cluster/health API when readonly', function () {
    if (!isCluster) {
      return;
    }

    let resp = arango.GET('/_admin/cluster/health');
    expect(resp.code).to.equal(200);
    expect(resp).to.have.property('ClusterId');
    expect(resp).to.have.property('Health');
  });

  it('can still access cluster/nodeVersion API when readonly', function () {
    if (!isCluster) {
      return;
    }

    let server = setReadOnlyAndGetDBServer();
    let resp = arango.GET_RAW('/_admin/cluster/nodeVersion?ServerID=' + server);
    expect(resp.code).to.equal(200);
  });

  it('can still access cluster/nodeEngine API when readonly', function () {
    if (!isCluster) {
      return;
    }

    let server = setReadOnlyAndGetDBServer();
    let resp = arango.GET_RAW('/_admin/cluster/nodeEngine?ServerID=' + server);
    expect(resp.code).to.equal(200);
  });

  it('can still access cluster metrics by serverId when readonly', function () {
    if (!isCluster) {
      return;
    }

    let server = setReadOnlyAndGetDBServer();
    let resp = arango.GET_RAW(endpoint + '/_admin/metrics?serverId=' + server);
    expect(resp.code).to.equal(200);
    const body = typeof resp.body === 'string' ? resp.body : String(resp.body);
    expect(body).to.include('arangodb_server_statistics_server_uptime_total');
  });

  it('cannot create a database when readonly', function () {
    let resp = arango.PUT('/_admin/server/mode', { mode: 'readonly' });
    expect(resp.code).to.equal(200);
    waitForHeartbeat();

    expect(resp).to.have.property('mode', 'readonly');
    try {
      db._createDatabase('UnitTestsDatabaseReadOnly');
      fail();
    } catch (err) {
      expect(err.errorNum).to.equal(internal.errors.ERROR_FORBIDDEN.code);
    }
  });
});

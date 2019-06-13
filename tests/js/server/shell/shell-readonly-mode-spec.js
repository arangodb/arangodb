/*global describe, it, ArangoAgency, after, afterEach, instanceInfo */

////////////////////////////////////////////////////////////////////////////////
/// @brief cluster collection creation tests
///
/// @file
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andreas Streichardt
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

const expect = require('chai').expect;

const internal = require('internal');
const db = internal.db;
const heartbeatInterval = 1; // 1 second

let isCluster = instanceInfo.arangods.length > 1;

let download;
if (isCluster) {
  download = require('internal').clusterDownload;
} else {
  download = require('internal').download;
}

let endpoint = instanceInfo.url; 

const waitForHeartbeat = function () {
  internal.wait(3 * heartbeatInterval, false); 
};

// this only tests the http api...there is a separate readonly test
describe('Readonly mode api', function() {
  afterEach(function() {
    // restore default server mode
    let resp = download(endpoint + '/_admin/server/mode', JSON.stringify({'mode': 'default'}), {
      method: 'put',
    });
      waitForHeartbeat();
  });
  
  after(function() {
    // wait for heartbeats so the "default" server mode has a chance to be picked up by all db servers
    // before we go on with other tests
    waitForHeartbeat();
  });

  it('outputs its current mode', function() {
    let resp = download(endpoint + '/_admin/server/mode');
    expect(resp.code).to.equal(200);
    let body = JSON.parse(resp.body);
    expect(body).to.have.property('mode', 'default');
  });

  it('can switch to readonly', function() {
    let resp = download(endpoint + '/_admin/server/mode', JSON.stringify({'mode': 'readonly'}), {
      method: 'put',
    });
    expect(resp.code).to.equal(200);
    waitForHeartbeat();
    let body = JSON.parse(resp.body);
    expect(body).to.have.property('mode', 'readonly');
  });

  it('throws an error when not passing an object', function() {
    let set = download(endpoint + '/_admin/server/mode', JSON.stringify('testi'), {
      method: 'put',
    });
    expect(set.code).to.equal(400);
    waitForHeartbeat();

    let resp = download(endpoint + '/_admin/server/mode');
    let body = JSON.parse(resp.body);
    expect(body).to.have.property('mode', 'default');
  });

  it('throws an error when passing an unknown mode', function() {
    let set = download(endpoint + '/_admin/server/mode', JSON.stringify({'mode': 'testi'}), {
      method: 'put',
    });
    expect(set.code).to.equal(400);
    waitForHeartbeat();

    let resp = download(endpoint + '/_admin/server/mode');
    let body = JSON.parse(resp.body);
    expect(body).to.have.property('mode', 'default');
  });

  it('the heartbeat should set readonly mode for all cluster nodes', function() {
    let resp = download(endpoint + '/_admin/server/mode', JSON.stringify({'mode': 'readonly'}), {
      method: 'put',
    });
    expect(resp.code).to.equal(200);
    waitForHeartbeat();
    
    let res = instanceInfo.arangods.filter(arangod => arangod.role === 'single' || arangod.role === 'coordinator' || arangod.role === 'primary')
    .every(arangod => {
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
});

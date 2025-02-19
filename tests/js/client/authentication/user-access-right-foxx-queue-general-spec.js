/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global describe, before, after, afterEach, it */

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
// / @author Mark Vollmary
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const expect = require('chai').expect;
const helper = require('@arangodb/testutils/user-helper');
const foxxManager = require('@arangodb/foxx/manager');
const fs = require('fs');
const internal = require('internal');
const basePath = fs.makeAbsolute(fs.join(internal.pathForTesting('common'), 'test-data', 'apps'));
const request = require('@arangodb/request');

const arangodb = require('@arangodb');
const arango = require('@arangodb').arango;
let connectionHandle = arango.getConnectionHandle();
const aql = arangodb.aql;
const db = internal.db;

describe('Foxx service', () => {

  const mount = '/queue_test_mount';

  before(() => {
    helper.switchUser("root");
    foxxManager.install(fs.join(basePath, 'queue'), mount);
  });

  after(() => {
    foxxManager.uninstall(mount, {force: true});
  });

  afterEach(() => {
    waitForJob();
    arango.DELETE_RAW(mount, '');
    // the job will create documents
    let cc = db._collection('foxx_queue_test');
    if (cc) {
      cc.truncate({ compact: false });
    }
  });

  it('should support queue registration', () => {
    const queuesBefore = db._query(aql`
      FOR queue IN _queues
      FILTER queue._key != 'default'
      RETURN queue
    `).toArray();
    const res = arango.POST_RAW(mount, '');
    expect(res.code).to.equal(204);
    const queuesAfter = db._query(aql`
      FOR queue IN _queues
      FILTER queue._key != 'default'
      RETURN queue
    `).toArray();
    expect(queuesAfter.length - queuesBefore.length).to.equal(1, 'Could not register Foxx queue');
  });

  it('should not register a queue two times', () => {
    const queuesBefore = db._queues.all().toArray();
    let res = arango.POST_RAW(mount, '');
    expect(res.code).to.equal(204);
    waitForJob();
    res = arango.POST_RAW(mount, '');
    expect(res.code).to.equal(204);
    const queuesAfter = db._queues.all().toArray();
    expect(queuesAfter.length - queuesBefore.length).to.equal(1);
  });

  it('should support jobs running in the queue', () => {
    let res = arango.POST(mount, '');
    expect(res.code).to.equal(204);
    expect(waitForJob()).to.equal(true, 'job from Foxx queue did not run!');
    const jobResult = db._query(aql`
      FOR i IN foxx_queue_test
        FILTER i.job == true
        RETURN 1
    `).toArray();
    expect(jobResult.length).to.equal(1);
  });

  it('should support jobs running in the queue', () => {
    let res = request.post(`${arango.getEndpoint().replace('tcp://', 'http://')}/${mount}`, {
      body: JSON.stringify({repeatTimes: 2})
    });
    expect(res.statusCode).to.equal(204);
    expect(waitForJob(2)).to.equal(true, 'job from foxx queue did not run!');
    const jobResult = db._query(aql`
      FOR i IN foxx_queue_test
        FILTER i.job == true
        RETURN 1
    `).toArray();
    expect(jobResult.length).to.equal(2);
  });

  it('should ignore the arango user', () => {
    let res = internal.download(`${arango.getEndpoint().replace('tcp://', 'http://')}/${mount}`, '', {
      method: 'post',
      username: 'root',
      password: ''
    });
    expect(res.code).to.equal(204);
    expect(waitForJob()).to.equal(true, 'job from foxx queue did not run!');
    const jobResult = db._query(aql`
      FOR i IN foxx_queue_test
        FILTER i.job == true
        RETURN 1
    `).toArray();
    expect(jobResult.length).to.equal(1);
  });

  const waitForJob = (runs = 1) => {
    let i = 0;
    while (i++ < 50) {
      internal.wait(0.1);
      const jobs = db._query(aql`
        FOR job IN _jobs
          FILTER job.type.mount == ${mount}
          RETURN job
      `).toArray();
      if (jobs.length === 1 && jobs[0].status === 'complete' && jobs[0].runs === runs) {
        return true;
      }
    }
    return false;
  };

});
after(() => {
  arango.connectHandle(connectionHandle);
  db._drop('UnitTestCollection');
  db._useDatabase('_system');
});

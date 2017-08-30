/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global describe, before, after, afterEach, it, require*/

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for user access rights
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2017 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / @author Mark Vollmary
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const expect = require('chai').expect;
const foxxManager = require('@arangodb/foxx/manager');
const fs = require('fs');
const internal = require('internal');
const basePath = fs.makeAbsolute(fs.join(internal.startupPath, 'common', 'test-data', 'apps'));
const download = internal.download;

const arangodb = require('@arangodb');
const arango = require('@arangodb').arango;
const aql = arangodb.aql;
const db = internal.db;

describe('Foxx service', () => {

  const mount = '/queue_test_mount';

  before(() => {
    foxxManager.install(fs.join(basePath, 'queue'), mount);
  });

  after(() => {
    foxxManager.uninstall(mount, {force: true});
  });

  afterEach(() => {
    download(`${arango.getEndpoint().replace('tcp://', 'http://')}/${mount}`, '', {
      method: 'delete'
    });
    // the job will create documents
    let cc = db._collection('foxx_queue_test');
    if (cc) {
      cc.truncate();
    }
  });

  it('should support queue registration', () => {
    const queuesBefore = db._query(aql`
      FOR queue IN _queues
      RETURN queue
    `).toArray();
    const res = download(`${arango.getEndpoint().replace('tcp://', 'http://')}/${mount}`, '', {
      method: 'post'
    });
    expect(res.code).to.equal(204);
    const queuesAfter = db._query(aql`
      FOR queue IN _queues
      RETURN queue
    `).toArray();
    expect(queuesAfter.length - queuesBefore.length).to.equal(1, 'Could not register foxx queue');
  });

  it('should not register a queue two times', () => {
    const queuesBefore = db._query(aql`
      FOR queue IN _queues
      RETURN queue
    `).toArray();
    let res = download(`${arango.getEndpoint().replace('tcp://', 'http://')}/${mount}`, '', {
      method: 'post'
    });
    expect(res.code).to.equal(204);
    res = download(`${arango.getEndpoint().replace('tcp://', 'http://')}/${mount}`, '', {
      method: 'post'
    });
    expect(res.code).to.equal(204);
    const queuesAfter = db._query(aql`
      FOR queue IN _queues
      RETURN queue
    `).toArray();
    expect(queuesAfter.length - queuesBefore.length).to.equal(1);
  });

  it('should support jobs running in the queue', () => {
    let res = download(`${arango.getEndpoint().replace('tcp://', 'http://')}/${mount}`, '', {
      method: 'post'
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

  const waitForJob = () => {
    let i = 0;
    while (i++ < 50) {
      internal.wait(0.1);
      const jobs = db._query(aql`
        FOR job IN _jobs
          FILTER job.type.mount == ${mount}
          RETURN job.status
      `).toArray();
      if (jobs.length === 1 && jobs[0] === 'complete') {
        return true;
      }
    }
    return false;
  };

});

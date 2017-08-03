/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global describe, before, after, it, require*/

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
// / @author Michael Hackstein
// / @author Mark Vollmary
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const expect = require('chai').expect;
const users = require('@arangodb/users');
const helper = require('@arangodb/user-helper');
const foxxManager = require('@arangodb/foxx/manager');
const dbName = helper.dbName;
const rightLevels = helper.rightLevels;
const errors = require('@arangodb').errors;
const fs = require('fs');
const basePath = fs.makeAbsolute(fs.join(require('internal').startupPath, 'common', 'test-data', 'apps'));
const download = require('internal').download;

const userSet = helper.userSet;
const systemLevel = helper.systemLevel;
const dbLevel = helper.dbLevel;
const colLevel = helper.colLevel;

const arangodb = require('@arangodb');
const arango = require('@arangodb').arango;
const aql = arangodb.aql;
const db = require('internal').db;
for (let l of rightLevels) {
  systemLevel[l] = new Set();
  dbLevel[l] = new Set();
  colLevel[l] = new Set();
}

const switchUser = (user, dbname) => {
  arango.reconnect(arango.getEndpoint(), dbname, user, '');
};

switchUser('root', '_system');
helper.removeAllUsers();

describe.skip('User Rights Management', () => {
  before(helper.generateAllUsers);
  after(helper.removeAllUsers);

  it('should check if all users are created', () => {
    switchUser('root', '_system');
    expect(userSet.size).to.equal(helper.userCount);
    for (let name of userSet) {
      expect(users.document(name), `Could not find user: ${name}`).to.not.be.undefined;
    }
  });

  it('should test rights for', () => {
    for (let name of userSet) {
      let canUse = false;
      try {
        switchUser(name, dbName);
        canUse = true;
      } catch (e) {
        canUse = false;
      }

      if (canUse) {
        describe(`user ${name}`, () => {
          const mount = `/${name}_mount`;
          before(() => {
            switchUser('root', dbName);
            foxxManager.install(fs.join(basePath, 'queue'), mount);
            switchUser(name, dbName);
            db._useDatabase(dbName);
          });

          after(() => {
            switchUser('root', dbName);
            download(`${arango.getEndpoint().replace('tcp://', 'http://')}/_db/${dbName}/${mount}`, '', {
              method: 'delete'
            });
            foxxManager.uninstall(mount, {force: true});
          });

          it('register a foxx queue/job', () => {
            if (dbLevel['rw'].has(name)) {
              const res = download(`${arango.getEndpoint().replace('tcp://', 'http://')}/_db/${dbName}/${mount}`, '', {
                method: 'post'
              });
              expect(res.code).to.equal(204);
              const queue = db._query(aql`
                FOR queue IN _queues
                FILTER queue._key == 'test_queue'
                RETURN queue.runAsUser
              `).toArray();
              expect(queue.length).to.equal(1, `${name} could not register foxx queue with sufficient rights`);
              expect(queue[0]).to.equal(name, `${name} could not register foxx queue with right runAsUser`);
              const job = db._query(aql`
                FOR job IN _jobs
                FILTER job.queue == 'test_queue'
                RETURN job
              `).toArray().length;
              expect(job).to.equal(1, `${name} could not register foxx job with sufficient rights`);
            } else {
              const res = download(`${arango.getEndpoint().replace('tcp://', 'http://')}/_db/${dbName}/${mount}`, '', {
                method: 'post'
              });
              expect(res.code).to.equal(403);
              const queue = db._query(aql`
                FOR queue IN _queues
                FILTER queue._key == 'test_queue'
                RETURN queue.runAsUser
              `).toArray();
              expect(queue.length).to.equal(0, `${name} could register foxx queue with insufficient rights`);
              const job = db._query(aql`
                FOR job IN _jobs
                FILTER job.queue == 'test_queue'
                RETURN job
              `).toArray().length;
              expect(job).to.equal(0, `${name} could  register foxx job with insufficient rights`);
            }
          });
        });
      }
    }
  });
});

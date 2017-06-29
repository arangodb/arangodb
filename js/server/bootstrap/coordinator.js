'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief initialize a new database
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
// / @author Dr. Frank Celler
// / @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief initialize a new database
// //////////////////////////////////////////////////////////////////////////////

(function () {
  const internal = require('internal');
  const errors = require('@arangodb').errors;

  // statistics can be turned off
  if (internal.enableStatistics && internal.threadNumber === 0) {
    try {
      require('@arangodb/statistics').startup();
    } catch (e) {
      if (e.errorNum === errors.ERROR_TASK_DUPLICATE_ID.code) {
        console.warn(e);
      } else {
        throw e;
      }
    }
  }

  // autoload all modules and reload routing information in all threads
  internal.loadStartup('server/bootstrap/autoload.js').startup();
  internal.loadStartup('server/bootstrap/routing.js').startup();

  if (internal.threadNumber === 0) {
    try {
      require('@arangodb/foxx/manager')._startup();
      try {
        require('@arangodb/tasks').register({
          id: 'self-heal',
          isSystem: true,
          period: 5 * 60, // secs
          command: function () {
            const FoxxManager = require('@arangodb/foxx/manager');
            FoxxManager.healAll();
          }
        });
      } catch (ee) {
        if (ee.errorNum === errors.ERROR_TASK_DUPLICATE_ID.code) {
          console.warn(ee);
        } else {
          throw ee;
        }
      }
      // start the queue manager once
      require('@arangodb/foxx/queues/manager').run();
      const systemCollectionsCreated = global.ArangoAgency.get('SystemCollectionsCreated');
      if (!(systemCollectionsCreated && systemCollectionsCreated.arango && systemCollectionsCreated.arango.SystemCollectionsCreated)) {
        // Wait for synchronous replication of system colls to settle:
        console.info('Waiting for initial replication of system collections...');
        const db = internal.db;
        const colls = db._collections().filter(c => c.name()[0] === '_');
        if (!require('@arangodb/cluster').waitForSyncRepl('_system', colls)) {
          throw new Error('System collections not properly set up. Refusing startup!');
        } else {
          global.ArangoAgency.set('SystemCollectionsCreated', true);
        }
      }
      console.info('bootstrapped coordinator %s', global.ArangoServerState.id());
    } catch (e) {
      console.error(e);
      return false;
    }
  }

  return true;
}());

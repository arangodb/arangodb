'use strict';

///////////////////////////////////////////////////////////////////////////////
// @brief initialize a new database
//
// @file
//
// DISCLAIMER
//
// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
//
// Licensed under the Apache License, Version 2.0 (the "License")
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright holder is ArangoDB GmbH, Cologne, Germany
//
// @author Dr. Frank Celler
// @author Copyright 2014-2020, ArangoDB GmbH, Cologne, Germany
///////////////////////////////////////////////////////////////////////////////

/// initializes a coordinator. will be called once per V8 context, on all coordinators
(function () {
  const internal = require('internal');

  // autoload all modules

  // the function name reloadRouting is misleading here, as it actually
  // only initializes/clears the local routing map, but doesn't rebuild
  // it.
  require('@arangodb/actions').reloadRouting();

  if (internal.threadNumber === 0) {
    try {
      require('@arangodb/foxx/manager')._startup();
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
      require('@arangodb').checkAvailableVersions();
    } catch (e) {
      console.error(e);
      return false;
    }
  }

  return true;
}());

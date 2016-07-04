'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief initialize foxxes for a database
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
// / @brief initialize foxxes
// //////////////////////////////////////////////////////////////////////////////

(function () {
  const internal = require('internal');
  const db = internal.db;
  return {
    foxx: function () {
      require('@arangodb/foxx/manager').initializeFoxx();
    },
    foxxes: function () {
      const console = require('console');
      const dbName = db._name();

      try {
        db._useDatabase('_system');
        const databases = db._databases();

        // loop over all databases
        for (const database of databases) {
          db._useDatabase(database);
          // and initialize Foxx applications
          try {
            require('@arangodb/foxx/manager').initializeFoxx();
          } catch (e) {
            console.errorLines(e.stack);
          }
        }
      } finally {
        // the caller does not need to know we changed the db
        db._useDatabase(dbName);
      }
    }
  };
}());

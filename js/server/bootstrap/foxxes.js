'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize foxxes for a database
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief initialize foxxes
////////////////////////////////////////////////////////////////////////////////

(function () {
  const internal = require('internal');
  const db = internal.db;
  return {
    foxx: function () {
      require('@arangodb/foxx/manager').initializeFoxx();
    },
    foxxes: function () {
      const aql = require('@arangodb').aql;
      const console = require('console');
      const dbName = db._name();
      const ttl = Number(internal.options()['server.session-timeout']) * 1000;
      const now = Date.now();

      try {
        db._useDatabase('_system');
        const databases = db._databases();

        // loop over all databases
        for (const database of databases) {
          db._useDatabase(database);
          // and initialize Foxx applications
          try {
            require('@arangodb/foxx/manager').initializeFoxx();
          }
          catch (e) {
            console.errorLines(e.stack);
          }

          // initialize sessions, too
          try {
            const cursor = db._query(aql`
              FOR s IN _sessions
              FOR u IN _users
              FILTER s.uid == u._id
              FILTER (s.lastAccess + ${ttl}) > ${now}
              RETURN {sid: s._key, user: u.user}
            `);
            while (cursor.hasNext()) {
              const doc = cursor.next();
              internal.createSid(doc.sid, doc.user);
            }
          } catch (e) {
            // we are intentionally ignoring errors here.
            // they can be caused by the _sessions collection not
            // being present in the current database etc.
            // if this happens, no sessions will be entered into the
            // session cache at startup, which is tolerable
            console.debugLines(e.stack);
          }
        }
      } finally {
        // the caller does not need to know we changed the db
        db._useDatabase(dbName);
      }
    }
  };
}());

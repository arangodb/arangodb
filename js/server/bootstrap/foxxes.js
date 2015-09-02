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

// -----------------------------------------------------------------------------
// --SECTION--                                                 initialize foxxes
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize foxxes
////////////////////////////////////////////////////////////////////////////////

(function () {
  var internal = require("internal");
  var db = internal.db;
  return {
    foxx: function () {
      require("org/arangodb/foxx/manager").initializeFoxx();
    },
    foxxes: function () {
      var dbName = db._name();

      try {
        db._useDatabase("_system");
        var databases = db._listDatabases();

        // loop over all databases
        for (var i = 0;  i < databases.length;  ++i) {
          db._useDatabase(databases[i]);
          // and initialize Foxx applications
          try {
            require("org/arangodb/foxx/manager").initializeFoxx();
          }
          catch (e) {
            require('console').error(e.stack);
          }

          // initialize sessions, too
          try {
            var query = "FOR s IN _sessions " + 
                        "FOR u IN _users " +
                        "FILTER s.uid == u._id " +
                        "RETURN { sid: s._key, user: u.user }";

            var cursor = db._query(query);
            while (cursor.hasNext()) {
              var doc = cursor.next();
              internal.createSid(doc.sid, doc.user);
            }
          } 
          catch (e) {
            // we are intentionally ignoring errors here.
            // they can be caused by the _sessions collection not
            // being present in the current database etc.
            // if this happens, no sessions will be entered into the
            // session cache at startup, which is tolerable
            require('console').debug(e.stack);
          }
        }
      }
      catch (e) {
        db._useDatabase(dbName);
        throw e;
      }
        
      // return to _system database so the caller does not need to know we changed the db
      db._useDatabase(dbName);
    }
  };
}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

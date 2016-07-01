'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief initialize routing
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
// / @brief initialize routing
// //////////////////////////////////////////////////////////////////////////////

(function () {
  var internal = require('internal');
  var console = require('console');
  return {
    startup: function () {
      var db = internal.db;
      var dbName = db._name();

      db._useDatabase('_system');
      var databases = db._databases();

      for (var i = 0; i < databases.length; ++i) {
        var name = databases[i];
        try {
          db._useDatabase(name);
          require('@arangodb/actions').reloadRouting();
        } catch (e) {
          console.info('trying to loading actions of a new database %s, ignored', name);
        }
      }

      // return to _system database so the caller does not need to know we changed the db
      db._useDatabase(dbName);
      return true;
    }
  };
}());

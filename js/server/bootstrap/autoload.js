'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief autoload modules
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
// / @brief autoload modules
// //////////////////////////////////////////////////////////////////////////////

(function () {
  let internal = require('internal');
  let console = require('console');
  let db = internal.db;

  return {
    // this functionality is deprecated and will be removed in 3.9
    startup: function () {
      if (!global.USE_OLD_SYSTEM_COLLECTIONS) {
        return;
      }

      const dbName = db._name();
      try {
        db._useDatabase('_system');
        let databases = db._databases();

        for (const name of databases) {
          try {
            db._useDatabase(name);
            internal.autoloadModules();
          } catch (e) {
            console.info('trying to autoload new database %s, ignored because of: %s', name, String(e));
          }
        }
      } finally {
        // return to _system database so the caller does not need to know we changed the db
        db._useDatabase(dbName);
      }

      return true;
    }
  };
}());

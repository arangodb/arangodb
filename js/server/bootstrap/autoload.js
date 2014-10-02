/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief autoload modules
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
// --SECTION--                                                  autoload modules
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief autoload modules
////////////////////////////////////////////////////////////////////////////////

(function () {
  "use strict";
  return {
    startup: function () {
      var internal = require("internal");
      var console = require("console");
      var db = internal.db;

      db._useDatabase("_system");

      var databases = db._listDatabases();
      var i;

      for (i = 0;  i < databases.length;  ++i) {
        var name = databases[i];

        try {
          db._useDatabase(name);
          internal.autoloadModules();
        }
        catch (err) {
          console.info("trying to autoload new database %s, ignored", name);
        }
      }

      return true;
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

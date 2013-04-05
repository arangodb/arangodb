/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true, unparam: true */
/*global exports, appCollection*/

////////////////////////////////////////////////////////////////////////////////
/// @brief A TODO-List Foxx-Application written for ArangoDB
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2013 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

(function () {
  "use strict";
  
  // Define the Repository
  var Foxx = require("org/arangodb/foxx");
  
  exports.Repository = Foxx.Repository.extend({
    // Define the functionality to create a new foxx
    store: function (content) {
      throw {
        code: 501,
        message: "To be implemented."
      };
    },
    
    // Define the functionality to uninstall an installed foxx
    uninstall: function (key) {
      return Foxx.uninstallApp(key);
    },
    
    // Define the functionality to deactivate an installed foxx.
    deactivate: function (key) {
      throw {
        code: 501,
        message: "To be implemented."
      };
    },
    
    // Define the functionality to activate an installed foxx.
    activate: function (key) {
      throw {
        code: 501,
        message: "To be implemented."
      };
    },
    
    // Define the functionality to display all foxxes
    viewAll: function () {
      return require("internal").db._collection("_aal").toArray();
    },
    
    // Define the functionality to update one foxx.
    update: function (id, content) {
      throw {
        code: 501,
        message: "To be implemented."
      };
    }
  });
}());

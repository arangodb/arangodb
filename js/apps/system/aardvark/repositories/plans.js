/*jslint indent: 2, nomen: true, maxlen: 100 */
/*global require, exports*/

////////////////////////////////////////////////////////////////////////////////
/// @brief A TODO-List Foxx-Application written for ArangoDB
///
/// @file This Document represents the repository communicating with ArangoDB
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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

  var _ = require("underscore"),
    Foxx = require("org/arangodb/foxx"),
    Plans;

  Plans = Foxx.Repository.extend({
    hasConfig: function() {
      if (this.loadConfig()) {
        return true;
      }
      return false;
    },

    clear: function() {
      this.collection.truncate();
    },

    loadConfig: function() {
      return this.collection.any();
    },

    getPlan: function() {
      return this.loadConfig().plan;
    },

    getRunInfo: function() {
      return this.loadConfig().runInfo;
    },

    updateConfig: function(config) {
      var old = this.loadConfig();
      this.collection.update(old._id, config);
      return true;
    },

    storeConfig: function(config) {
      this.collection.truncate();
      this.collection.save(config);
      return true;
    }
  });

  exports.Repository = Plans;
}());

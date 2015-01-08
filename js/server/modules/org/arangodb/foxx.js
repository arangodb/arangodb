/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief The Foxx Framework
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Lucas Dohmen
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var Controller = require("org/arangodb/foxx/controller").Controller,
  Model = require("org/arangodb/foxx/model").Model,
  Repository = require("org/arangodb/foxx/repository").Repository,
  queues = require("org/arangodb/foxx/queues"),
  createQuery = require("org/arangodb/foxx/query").createQuery,
  manager = require("org/arangodb/foxx/manager"),
  toJSONSchema = require("org/arangodb/foxx/schema").toJSONSchema,
  arangodb = require("org/arangodb");

exports.Controller = Controller;
exports.Model = Model;
exports.Repository = Repository;
exports.queues = queues;
exports.createQuery = createQuery;
exports.toJSONSchema = toJSONSchema;
exports.requireApp = function (path) {
  'use strict';
  return manager.mountedApp(arangodb.normalizeURL('/' + path));
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:

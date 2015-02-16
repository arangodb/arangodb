/*jslint indent: 2, nomen: true, maxlen: 100 */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief A configuration model for foxx templates
///
/// @file
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
(function() {
  "use strict";

  var Foxx = require("org/arangodb/foxx"),
    joi = require("joi"),
    Configuration;

  Configuration = Foxx.Model.extend({
    schema: {
      applicationContext: joi.string().optional(),
      path: joi.string().optional(),
      name: joi.string().required(),
      collectionNames: joi.array().required(),
      authenticated: joi.boolean().required(),
      author: joi.string().required(),
      description: joi.string().required(),
      license: joi.string().required()
    }
  });

  exports.Model = Configuration;
}());

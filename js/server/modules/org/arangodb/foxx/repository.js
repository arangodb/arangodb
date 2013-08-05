/*jslint indent: 2, nomen: true, maxlen: 120 */
/*global module, require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Repository
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

var Repository,
  _ = require("underscore"),
  backbone_helpers = require("backbone");

// -----------------------------------------------------------------------------
// --SECTION--                                                        Repository
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_initializer
/// @brief Create a new instance of Repository
///
/// @FUN{new FoxxRepository(@FA{prefix}, @FA{collection}, @FA{model})}
///
/// Create a new instance of Repository
/// 
/// A Foxx Repository is always initialized with the prefix, the collection and
/// the modelPrototype.  If you initialize a model, you can give it initial data
/// as an object.
///
/// @EXAMPLES
///
/// @code
///     instance = new Repository(prefix, collection, modelPrototype);
/// @endcode
////////////////////////////////////////////////////////////////////////////////

Repository = function (prefix, collection, modelPrototype) {
  'use strict';

  this.prefix = prefix;
  this.collection = collection;
  this.modelPrototype = modelPrototype;
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_extend
/// @brief Extend the Repository prototype to add or overwrite methods.
///
/// Extend the Repository prototype to add or overwrite methods.
////////////////////////////////////////////////////////////////////////////////

Repository.extend = backbone_helpers.extend;

exports.Repository = Repository;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:

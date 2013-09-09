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
/// @FUN{new FoxxRepository(@FA{collection}, @FA{opts})}
///
/// Create a new instance of Repository
///
/// A Foxx Repository is always initialized with a collection object. You can
/// your collection object by asking your Foxx.Controller for it via the
/// `collection` method that takes the name of the collection (and will prepend
/// the prefix of your application). It also takes two optional arguments:
///
/// 1. Model: The prototype of a model. If you do not provide it, it will default
/// to Foxx.Model
/// 2. Prefix: You can provide the prefix of the application if you need it in
/// your Repository (for some AQL queries probably). Get it from the Foxx.Controller
/// via the `collectionPrefix` attribute.
///
/// @EXAMPLES
///
/// @code
///     instance = new Repository(app.collection("my_collection"));
///     // or:
///     instance = new Repository(app.collection("my_collection"), {
///       model: MyModelPrototype,
///       prefix: app.collectionPrefix,
///     });
/// @endcode
////////////////////////////////////////////////////////////////////////////////

Repository = function (collection, opts) {
  'use strict';
  var options = opts || {};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_collection
/// @brief The collection object.
///
/// See the documentation of collection.
////////////////////////////////////////////////////////////////////////////////

  this.collection = collection;

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_modelPrototype
/// @brief The prototype of the according model.
///
/// See the documentation of collection.
////////////////////////////////////////////////////////////////////////////////

  this.modelPrototype = options.model || require("org/arangodb/foxx/model").Model;

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_prefix
/// @brief The prefix of the application.
///
/// See the documentation of collection.
////////////////////////////////////////////////////////////////////////////////

  this.prefix = options.prefix;
};

_.extend(Repository.prototype, {
////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_save
/// @brief See the documentation of collection.
///
/// See the documentation of collection.
////////////////////////////////////////////////////////////////////////////////
  save: function () {
    'use strict';
    this.collection.save.apply(this.collection, arguments);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_remove
/// @brief See the documentation of collection.
///
/// See the documentation of collection.
////////////////////////////////////////////////////////////////////////////////
  remove: function () {
    'use strict';
    this.collection.remove.apply(this.collection, arguments);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_replace
/// @brief See the documentation of collection.
///
/// See the documentation of collection.
////////////////////////////////////////////////////////////////////////////////
  replace: function () {
    'use strict';
    this.collection.replace.apply(this.collection, arguments);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_update
/// @brief See the documentation of collection.
///
/// See the documentation of collection.
////////////////////////////////////////////////////////////////////////////////
  update: function () {
    'use strict';
    this.collection.update.apply(this.collection, arguments);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_removeByExample
/// @brief See the documentation of collection.
///
/// See the documentation of collection.
////////////////////////////////////////////////////////////////////////////////
  removeByExample: function () {
    'use strict';
    this.collection.removeByExample.apply(this.collection, arguments);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_replaceByExample
/// @brief See the documentation of collection.
///
/// See the documentation of collection.
////////////////////////////////////////////////////////////////////////////////
  replaceByExample: function () {
    'use strict';
    this.collection.replaceByExample.apply(this.collection, arguments);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_updateByExample
/// @brief See the documentation of collection.
///
/// See the documentation of collection.
////////////////////////////////////////////////////////////////////////////////
  updateByExample: function () {
    'use strict';
    this.collection.updateByExample.apply(this.collection, arguments);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_byExample
/// @brief See the documentation of collection.
///
/// See the documentation of collection.
////////////////////////////////////////////////////////////////////////////////
  byExample: function () {
    'use strict';
    this.collection.byExample.apply(this.collection, arguments);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_firstExample
/// @brief See the documentation of collection.
///
/// See the documentation of collection.
////////////////////////////////////////////////////////////////////////////////
  firstExample: function () {
    'use strict';
    this.collection.firstExample.apply(this.collection, arguments);
  }
});

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

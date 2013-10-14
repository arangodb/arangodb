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
/// A Foxx Repository is always initialized with a collection object. You can get
/// your collection object by asking your Foxx.Controller for it: the
/// `collection` method takes the name of the collection (and will prepend
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
  this.options = opts || {};

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

  this.modelPrototype = this.options.model || require("org/arangodb/foxx/model").Model;

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_prefix
/// @brief The prefix of the application.
///
/// See the documentation of collection.
////////////////////////////////////////////////////////////////////////////////

  this.prefix = this.options.prefix;
};

_.extend(Repository.prototype, {
////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_save
/// @brief Save a model into the database
///
/// Expects a model. Will set the ID and Rev on the model.
/// Returns the model (for convenience).
////////////////////////////////////////////////////////////////////////////////
  save: function (model) {
    'use strict';
    var id_and_rev = this.collection.save(model.forDB());
    model.set(id_and_rev);
    return model;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_removeById
/// @brief Remove the document with the given ID
///
/// Expects an ID of an existing document.
////////////////////////////////////////////////////////////////////////////////
  removeById: function (id) {
    'use strict';
    this.collection.remove(id);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_removeByExample
/// @brief Remove all documents that match the example
///
/// Find all documents that fit this example and remove them.
////////////////////////////////////////////////////////////////////////////////
  removeByExample: function (example) {
    'use strict';
    this.collection.removeByExample(example);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_replace
/// @brief Replace a model in the database
///
/// Find the model in the database by its `_id` and replace it with this version.
/// Expects a model. Sets the Revision of the model.
/// Returns the model (for convenience).
////////////////////////////////////////////////////////////////////////////////
  replace: function (model) {
    'use strict';
    var id = model.get("_id"),
      data = model.forDB(),
      id_and_rev = this.collection.replace(id, data);
    model.set(id_and_rev);
    return model;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_replaceById
/// @brief Find an item by ID and replace it with the given model
///
/// Find the model in the database by the given ID and replace it with the given.
/// model.
/// Expects a model. Sets the ID and Revision of the model.
/// Returns the model (for convenience).
////////////////////////////////////////////////////////////////////////////////
  replaceById: function (id, model) {
    'use strict';
    var data = model.forDB(),
      id_and_rev = this.collection.replace(id, data);
    model.set(id_and_rev);
    return model;
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_byId
/// @brief Find a model by its ID
///
/// Returns the model for the given ID.
////////////////////////////////////////////////////////////////////////////////
  byId: function (id) {
    'use strict';
    var data = this.collection.document(id);
    return (new this.modelPrototype(data));
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_byExample
/// @brief Find all models by an example
///
/// Returns an array of models for the given ID.
////////////////////////////////////////////////////////////////////////////////
  byExample: function (example) {
    'use strict';
    var rawDocuments = this.collection.byExample(example).toArray();
    return _.map(rawDocuments, function (rawDocument) {
      return (new this.modelPrototype(rawDocument));
    }, this);
  },

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_repository_firstExample
/// @brief Find the first model that matches the example.
///
/// Returns a model that matches the given example.
////////////////////////////////////////////////////////////////////////////////
  firstExample: function (example) {
    'use strict';
    var rawDocument = this.collection.firstExample(example);
    return (new this.modelPrototype(rawDocument));
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

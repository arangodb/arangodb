/*jslint indent: 2, nomen: true, maxlen: 120, white: false, sloppy: false */
/*global module, require, exports */

////////////////////////////////////////////////////////////////////////////////
/// Foxx Repository
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

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_initializer
/// Create a new instance of Repository
///
/// `new FoxxRepository(collection, opts)`
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
/// your Repository (for some AQL queries probably).
///
/// *Examples*
///
/// ```javascript
/// instance = new Repository(appContext.collection("my_collection"));
/// // or:
/// instance = new Repository(appContext.collection("my_collection"), {
///   model: MyModelPrototype,
///   prefix: app.collectionPrefix,
/// });
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Repository = function (collection, opts) {
  'use strict';
  this.options = opts || {};

// -----------------------------------------------------------------------------
// --SECTION--                                                        Attributes
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_collection
/// The wrapped ArangoDB collection object
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  this.collection = collection;

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_modelPrototype
/// The prototype of the according model.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  this.modelPrototype = this.options.model || require("org/arangodb/foxx/model").Model;

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_prefix
/// The prefix of the application. This is useful if you want to construct AQL
/// queries for example.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  this.prefix = this.options.prefix;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                           Methods
// -----------------------------------------------------------------------------

_.extend(Repository.prototype, {
// -----------------------------------------------------------------------------
// --SUBSECTION--                                                 Adding Entries
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_save
/// `save(model)`
///
/// Save a model into the database
///
/// Expects a model. Will set the ID and Rev on the model.
/// Returns the model.
///
/// *Examples*
///
/// ```javascript
/// repository.save(my_model);
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  save: function (model) {
    'use strict';
    var id_and_rev = this.collection.save(model.forDB());
    model.set(id_and_rev);
    return model;
  },

// -----------------------------------------------------------------------------
// --SUBSECTION--                                                Finding Entries
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_byId
/// `byId(id)`
///
/// Find a model by its ID
///
/// Returns the model for the given ID.
///
/// *Examples*
///
/// ```javascript
/// var myModel = repository.byId('test/12411');
/// myModel.get('name');
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  byId: function (id) {
    'use strict';
    var data = this.collection.document(id);
    return (new this.modelPrototype(data));
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_byExample
/// `byExample(example)`
///
/// Find all models by an example
///
/// Returns an array of models for the given ID.
///
/// *Examples*
///
/// ```javascript
/// var myModel = repository.byExample({ amazing: true });
/// myModel[0].get('name');
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  byExample: function (example) {
    'use strict';
    var rawDocuments = this.collection.byExample(example).toArray();
    return _.map(rawDocuments, function (rawDocument) {
      return (new this.modelPrototype(rawDocument));
    }, this);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_firstExample
/// `firstExample(example)`
///
/// Find the first model that matches the example.
///
/// Returns a model that matches the given example.
///
/// *Examples*
///
/// ```javascript
/// var myModel = repository.firstExample({ amazing: true });
/// myModel.get('name');
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  firstExample: function (example) {
    'use strict';
    var rawDocument = this.collection.firstExample(example);
    return (new this.modelPrototype(rawDocument));
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_all
/// `all()`
///
/// Returns an array of models that matches the given example. You need to provide
/// both a skip and a limit value.
/// **Warning:** ArangoDB doesn't guarantee a specific order in this case, to make
/// this really useful we have to explicitly provide something to order by.
///
/// *Examples*
///
/// ```javascript
/// var myModel = repository.all({ skip: 4, limit: 2 });
/// myModel[0].get('name');
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  all: function (options) {
    'use strict';
    var rawDocuments = this.collection.all().skip(options.skip).limit(options.limit).toArray();
    return _.map(rawDocuments, function (rawDocument) {
      return (new this.modelPrototype(rawDocument));
    }, this);
  },

// -----------------------------------------------------------------------------
// --SUBSECTION--                                               Removing Entries
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_remove
/// `remove(model)`
///
/// Remove the model from the repository
///
/// Expects a model
///
/// *Examples*
///
/// ```javascript
/// repository.remove(myModel);
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  remove: function (model) {
    'use strict';
    var id = model.get('_id');
    this.collection.remove(id);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_removeById
/// `removeById(id)`
///
/// Remove the document with the given ID
///
/// Expects an ID of an existing document.
///
/// *Examples*
///
/// ```javascript
/// repository.removeById('test/12121');
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  removeById: function (id) {
    'use strict';
    this.collection.remove(id);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_removeByExample
/// `removeByExample(example)`
///
/// Remove all documents that match the example
///
/// Find all documents that fit this example and remove them.
///
/// *Examples*
///
/// ```javascript
/// repository.removeByExample({ toBeDeleted: true });
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  removeByExample: function (example) {
    'use strict';
    this.collection.removeByExample(example);
  },

// -----------------------------------------------------------------------------
// --SUBSECTION--                                              Replacing Entries
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_replace
/// `replace(model)`
///
/// Find the model in the database by its `_id` and replace it with this version.
/// Expects a model. Sets the Revision of the model.
/// Returns the model.
///
/// *Examples*
///
/// ```javascript
/// myModel.set('name', 'Jan Steemann');
/// repository.replace(myModel);
/// ```
/// @endDocuBlock
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
/// @startDocuBlock JSF_foxx_repository_replaceById
/// `replaceById(id, model)`
///
/// Find an item by ID and replace it with the given model
///
/// Find the model in the database by the given ID and replace it with the given.
/// model.
/// Sets the ID and Revision of the model and also returns it.
///
/// *Examples*
///
/// ```javascript
/// repository.replaceById('test/123345', myNewModel);
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  replaceById: function (id, model) {
    'use strict';
    var data = model.forDB(),
      id_and_rev = this.collection.replace(id, data);
    model.set(id_and_rev);
    return model;
  }

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_replaceByExample
/// `replaceByExample(example, model)`
///
/// Find an item by example and replace it with the given model
///
/// Find the model in the database by the given example and replace it with the given.
/// model.
/// Sets the ID and Revision of the model and also returns it.
///
/// *Examples*
///
/// ```javascript
/// repository.replaceByExample({ replaceMe: true }, myNewModel);
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SUBSECTION--                                               Updating Entries
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_updateById
/// `updateById(id, object)`
///
/// Find an item by ID and update it with the attributes in the provided object.
/// Returns the updated model.
///
/// *Examples*
///
/// ```javascript
/// repository.updateById('test/12131', { newAttribute: 'awesome' });
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_updateByExample
/// `updateByExample(example, object)`
///
/// Find an item by example and update it with the attributes in the provided object.
/// Returns the updated model.
///
/// *Examples*
///
/// ```javascript
/// repository.updateByExample({ findMe: true }, { newAttribute: 'awesome' });
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SUBSECTION--                                               Counting Entries
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_count
/// `count()`
///
/// Return the number of entries in this collection
///
/// *Examples*
///
/// ```javascript
/// repository.count();
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
});

Repository.extend = backbone_helpers.extend;

exports.Repository = Repository;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:

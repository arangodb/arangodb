/*global require, exports */

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
  Model = require("org/arangodb/foxx/model").Model,
  _ = require("underscore"),
  extend = require('org/arangodb/extend').extend;

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_initializer
/// `new FoxxRepository(collection, opts)`
///
/// Create a new instance of Repository.
///
/// A Foxx Repository is always initialized with a collection object. You can get
/// your collection object by asking your Foxx.Controller for it: the
/// *collection* method takes the name of the collection (and will prepend
/// the prefix of your application). It also takes two optional arguments:
///
/// 1. Model: The prototype of a model. If you do not provide it, it will default
/// to Foxx.Model
/// 2. Prefix: You can provide the prefix of the application if you need it in
/// your Repository (for some AQL queries probably)
///
/// @EXAMPLES
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
/// The wrapped ArangoDB collection object.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  this.collection = collection;

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_modelPrototype
/// The prototype of the according model.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  this.modelPrototype = this.options.model || Model;

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_prefix
/// The prefix of the application. This is useful if you want to construct AQL
/// queries for example.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  this.prefix = this.options.prefix;

  // Undocumented, unfinished graph feature
  this.graph = this.options.graph;

  if (this.indexes) {
    _.each(this.indexes, function (index) {
      this.collection.ensureIndex(index);
    }, this);
  }
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
/// `FoxxRepository#save(model)`
///
/// Saves a model into the database.
/// Expects a model. Will set the ID and Rev on the model.
/// Returns the model.
///
/// @EXAMPLES
///
/// ```javascript
/// repository.save(my_model);
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  save: function (model) {
    'use strict';
    model.emit('beforeCreate');
    model.emit('beforeSave');
    var id_and_rev = this.collection.save(model.forDB());
    model.set(id_and_rev);
    model.emit('afterSave');
    model.emit('afterCreate');
    return model;
  },

// -----------------------------------------------------------------------------
// --SUBSECTION--                                                Finding Entries
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_byId
/// `FoxxRepository#byId(id)`
///
/// Returns the model for the given ID.
///
/// @EXAMPLES
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
/// `FoxxRepository#byExample(example)`
///
/// Returns an array of models for the given ID.
///
/// @EXAMPLES
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
/// `FoxxRepository#firstExample(example)`
///
/// Returns the first model that matches the given example.
///
/// @EXAMPLES
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
/// `FoxxRepository#all()`
///
/// Returns an array of models that matches the given example. You can provide
/// both a skip and a limit value.
///
/// **Warning:** ArangoDB doesn't guarantee a specific order in this case, to make
/// this really useful we have to explicitly provide something to order by.
///
/// *Parameter*
///
/// * *options* (optional):
///   * *skip* (optional): skips the first given number of models.
///   * *limit* (optional): only returns at most the given number of models.
///
/// @EXAMPLES
///
/// ```javascript
/// var myModel = repository.all({ skip: 4, limit: 2 });
/// myModel[0].get('name');
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  all: function (options) {
    'use strict';
    if (!options) {
      options = {};
    }
    var rawDocuments = this.collection.all();
    if (options.skip) {
      rawDocuments = rawDocuments.skip(options.skip);
    }
    if (options.limit) {
      rawDocuments = rawDocuments.limit(options.limit);
    }
    return _.map(rawDocuments.toArray(), function (rawDocument) {
      return (new this.modelPrototype(rawDocument));
    }, this);
  },

// -----------------------------------------------------------------------------
// --SUBSECTION--                                               Removing Entries
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_remove
/// `FoxxRepository#remove(model)`
///
/// Remove the model from the repository.
/// Expects a model.
///
/// @EXAMPLES
///
/// ```javascript
/// repository.remove(myModel);
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  remove: function (model) {
    'use strict';
    var id = model.get('_id');
    return this.collection.remove(id);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_removeById
/// `FoxxRepository#removeById(id)`
///
/// Remove the document with the given ID.
/// Expects an ID of an existing document.
///
/// @EXAMPLES
///
/// ```javascript
/// repository.removeById('test/12121');
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  removeById: function (id) {
    'use strict';
    return this.collection.remove(id);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_removeByExample
/// `FoxxRepository#removeByExample(example)`
///
/// Find all documents that fit this example and remove them.
///
/// @EXAMPLES
///
/// ```javascript
/// repository.removeByExample({ toBeDeleted: true });
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  removeByExample: function (example) {
    'use strict';
    return this.collection.removeByExample(example);
  },

// -----------------------------------------------------------------------------
// --SUBSECTION--                                              Replacing Entries
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_replace
/// `FoxxRepository#replace(model)`
///
/// Find the model in the database by its *_id* and replace it with this version.
/// Expects a model. Sets the revision of the model.
/// Returns the model.
///
/// @EXAMPLES
///
/// ```javascript
/// myModel.set('name', 'Jan Steemann');
/// repository.replace(myModel);
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  replace: function (model) {
    'use strict';
    var id = model.get("_id") || model.get("_key"),
      data = model.forDB(),
      id_and_rev = this.collection.replace(id, data);
    model.set(id_and_rev);
    return model;
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_replaceById
/// `FoxxRepository#replaceById(id, object)`
///
/// Find the item in the database by the given ID and replace it with the
/// given object's attributes.
///
/// If the object is a model, updates the model's revision and returns the model.
///
/// @EXAMPLES
///
/// ```javascript
/// repository.replaceById('test/123345', myNewModel);
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  replaceById: function (id, data) {
    'use strict';
    if (data instanceof Model) {
      var id_and_rev = this.collection.replace(id, data.forDB());
      data.set(id_and_rev);
      return data;
    }
    return this.collection.replace(id, data);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_replaceByExample
/// `FoxxRepository#replaceByExample(example, object)`
///
/// Find every matching item by example and replace it with the attributes in
/// the provided object.
///
/// @EXAMPLES
///
/// ```javascript
/// repository.replaceByExample({ replaceMe: true }, myNewModel);
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  replaceByExample: function (example, data) {
    'use strict';
    return this.collection.replaceByExample(example, data);
  },

// -----------------------------------------------------------------------------
// --SUBSECTION--                                               Updating Entries
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_update
/// `FoxxRepository#update(model, object)`
///
/// Find the model in the database by its *_id* and update it with the given object.
/// Expects a model. Sets the revision of the model and updates its properties.
/// Returns the model.
///
/// @EXAMPLES
///
/// ```javascript
/// repository.update(myModel, {name: 'Jan Steeman'});
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  update: function (model, data) {
    'use strict';
    var id = model.get("_id") || model.get("_key"),
      id_and_rev = this.collection.update(id, data);
    model.set(data);
    model.set(id_and_rev);
    return model;
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_updateById
/// `FoxxRepository#updateById(id, object)`
///
/// Find an item by ID and update it with the attributes in the provided object.
///
/// If the object is a model, updates the model's revision and returns the model.
///
/// @EXAMPLES
///
/// ```javascript
/// repository.updateById('test/12131', { newAttribute: 'awesome' });
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  updateById: function (id, data) {
    'use strict';
    if (data instanceof Model) {
      var id_and_rev = this.collection.update(id, data.forDB());
      data.set(id_and_rev);
      return data;
    }
    return this.collection.update(id, data);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_updateByExample
/// `FoxxRepository#updateByExample(example, object)`
///
/// Find every matching item by example and update it with the attributes in
/// the provided object.
///
/// @EXAMPLES
///
/// ```javascript
/// repository.updateByExample({ findMe: true }, { newAttribute: 'awesome' });
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  updateByExample: function (example, data) {
    'use strict';
    return this.collection.updateByExample(example, data);
  },

// -----------------------------------------------------------------------------
// --SUBSECTION--                                               Counting Entries
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_count
/// `FoxxRepository#count()`
///
/// Returns the number of entries in this collection.
///
/// @EXAMPLES
///
/// ```javascript
/// repository.count();
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
  count: function () {
    'use strict';
    return this.collection.count();
  }
});

var indexPrototypes = {
  skiplist: {

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_range
/// `FoxxRepository#range(attribute, left, right)`
///
/// Returns all models in the repository such that the attribute is greater
/// than or equal to *left* and strictly less than *right*.
///
/// For range queries it is required that a skiplist index is present for the
/// queried attribute. If no skiplist index is present on the attribute, the
/// method will not be available.
///
/// *Parameter*
///
/// * *attribute*: attribute to query.
/// * *left*: lower bound of the value range (inclusive).
/// * *right*: upper bound of the value range (exclusive).
///
/// @EXAMPLES
///
/// ```javascript
/// repository.range("age", 10, 13);
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
    range: function (attribute, left, right) {
      'use strict';
      var rawDocuments = this.collection.range(attribute, left, right).toArray();
      return _.map(rawDocuments, function (rawDocument) {
        return (new this.modelPrototype(rawDocument));
      }, this);
    }
  },
  geo: {

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_near
/// `FoxxRepository#near(latitude, longitude, options)`
///
/// Finds models near the coordinate *(latitude, longitude)*. The returned
/// list is sorted by distance with the nearest model coming first.
///
/// For geo queries it is required that a geo index is present in the
/// repository. If no geo index is present, the methods will not be available.
///
/// *Parameter*
///
/// * *latitude*: latitude of the coordinate.
/// * *longitude*: longitude of the coordinate.
/// * *options* (optional):
///   * *geo* (optional): name of the specific geo index to use.
///   * *distance* (optional): If set to a truthy value, the returned models
///     will have an additional property containing the distance between the
///     given coordinate and the model. If the value is a string, that value
///     will be used as the property name, otherwise the name defaults to *"distance"*.
///   * *limit* (optional): number of models to return. Defaults to *100*.
///
/// @EXAMPLES
///
/// ```javascript
/// repository.near(0, 0, {geo: "home", distance: true, limit: 10});
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
    near: function (latitude, longitude, options) {
      'use strict';
      var collection = this.collection,
        rawDocuments;
      if (!options) {
        options = {};
      }
      if (options.geo) {
        collection = collection.geo(options.geo);
      }
      rawDocuments = collection.near(latitude, longitude);
      if (options.distance) {
        rawDocuments = rawDocuments.distance();
      }
      if (options.limit) {
        rawDocuments = rawDocuments.limit(options.limit);
      }
      return _.map(rawDocuments.toArray(), function (rawDocument) {
        var model = (new this.modelPrototype(rawDocument)),
          distance;
        if (options.distance) {
          delete model.attributes._distance;
          distance = typeof options.distance === "string" ? options.distance : "distance";
          model[distance] = rawDocument._distance;
        }
        return model;
      }, this);
    },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_within
/// `FoxxRepository#within(latitude, longitude, radius, options)`
///
/// Finds models within the distance *radius* from the coordinate
/// *(latitude, longitude)*. The returned list is sorted by distance with the
/// nearest model coming first.
///
/// For geo queries it is required that a geo index is present in the
/// repository. If no geo index is present, the methods will not be available.
///
/// *Parameter*
///
/// * *latitude*: latitude of the coordinate.
/// * *longitude*: longitude of the coordinate.
/// * *radius*: maximum distance from the coordinate.
/// * *options* (optional):
///   * *geo* (optional): name of the specific geo index to use.
///   * *distance* (optional): If set to a truthy value, the returned models
///     will have an additional property containing the distance between the
///     given coordinate and the model. If the value is a string, that value
///     will be used as the property name, otherwise the name defaults to *"distance"*.
///   * *limit* (optional): number of models to return. Defaults to *100*.
///
/// @EXAMPLES
///
/// ```javascript
/// repository.within(0, 0, 2000 * 1000, {geo: "home", distance: true, limit: 10});
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
    within: function (latitude, longitude, radius, options) {
      'use strict';
      var collection = this.collection,
        rawDocuments;
      if (!options) {
        options = {};
      }
      if (options.geo) {
        collection = collection.geo(options.geo);
      }
      rawDocuments = collection.within(latitude, longitude, radius);
      if (options.distance) {
        rawDocuments = rawDocuments.distance();
      }
      if (options.limit) {
        rawDocuments = rawDocuments.limit(options.limit);
      }
      return _.map(rawDocuments.toArray(), function (rawDocument) {
        var model = (new this.modelPrototype(rawDocument)),
          distance;
        if (options.distance) {
          delete model.attributes._distance;
          distance = typeof options.distance === "string" ? options.distance : "distance";
          model[distance] = rawDocument._distance;
        }
        return model;
      }, this);
    }
  },
  fulltext: {

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_repository_fulltext
/// `FoxxRepository#fulltext(attribute, query, options)`
///
/// Returns all models whose attribute *attribute* matches the search query
/// *query*.
///
/// In order to use the fulltext method, a fulltext index must be defined on
/// the repository. If multiple fulltext indexes are defined on the repository
/// for the attribute, the most capable one will be selected.
/// If no fulltext index is present, the method will not be available.
///
/// *Parameter*
///
/// * *attribute*: model attribute to perform a search on.
/// * *query*: query to match the attribute against.
/// * *options* (optional):
///   * *limit* (optional): number of models to return. Defaults to all.
///
/// @EXAMPLES
///
/// ```javascript
/// repository.fulltext("text", "word", {limit: 1});
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
    fulltext: function (attribute, query, options) {
      'use strict';
      if (!options) {
        options = {};
      }
      var rawDocuments = this.collection.fulltext(attribute, query);
      if (options.limit) {
        rawDocuments = rawDocuments.limit(options.limit);
      }
      return _.map(rawDocuments.toArray(), function (rawDocument) {
        return (new this.modelPrototype(rawDocument));
      }, this);
    }
  }
};

var addIndexMethods = function (prototype) {
  'use strict';
  _.each(prototype.indexes, function (index) {
    var protoMethods = indexPrototypes[index.type];
    if (!protoMethods) {
      return;
    }
    _.each(protoMethods, function (method, key) {
      if (prototype[key] === undefined) {
        prototype[key] = method;
      }
    });
  });
};

Repository.extend = function (prototypeProperties, constructorProperties) {
  'use strict';
  var constructor = extend.call(this, prototypeProperties, constructorProperties);
  if (constructor.prototype.hasOwnProperty('indexes')) {
    addIndexMethods(constructor.prototype);
  }
  return constructor;
};

exports.Repository = Repository;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:

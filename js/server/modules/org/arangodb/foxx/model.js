'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Model
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

const _ = require('underscore');
const joi = require('joi');
const is = require('org/arangodb/is');
const extend = require('org/arangodb/extend').extend;
const EventEmitter = require('events').EventEmitter;
const metadataSchema = {
  _id: joi.string().optional(),
  _key: joi.string().optional(),
  _oldRev: joi.string().optional(),
  _rev: joi.string().optional(),
  _from: joi.string().optional(),
  _to: joi.string().optional()
};

// -----------------------------------------------------------------------------
// --SECTION--                                                             Model
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_model_initializer
///
/// `new FoxxModel(data)`
///
/// If you initialize a model, you can give it initial *data* as an object.
///
/// @EXAMPLES
///
/// ```js
/// instance = new Model({
///   a: 1
/// });
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function excludeExtraAttributes(attributes, model) {
  if (!model.schema) {
    return _.clone(attributes);
  }
  return _.omit(attributes, _.difference(
    _.keys(metadataSchema),
    _.keys(model.schema)
  ));
}

class Model extends EventEmitter {
  constructor(attributes) {
    super();

  ////////////////////////////////////////////////////////////////////////////////
  /// @startDocuBlock JSF_foxx_model_attributes
  ///
  /// `model.attributes`
  ///
  /// The *attributes* property is the internal hash containing the model's state.
  /// @endDocuBlock
  ////////////////////////////////////////////////////////////////////////////////

    this.attributes = {};

  ////////////////////////////////////////////////////////////////////////////////
  /// @startDocuBlock JSF_foxx_model_isvalid
  ///
  /// `model.isValid`
  ///
  /// The *isValid* flag indicates whether the model's state is currently valid.
  /// If the model does not have a schema, it will always be considered valid.
  /// @endDocuBlock
  ////////////////////////////////////////////////////////////////////////////////

    this.isValid = true;

  ////////////////////////////////////////////////////////////////////////////////
  /// @startDocuBlock JSF_foxx_model_errors
  ///
  /// `model.errors`
  ///
  /// The *errors* property maps the names of any invalid attributes to their
  /// corresponding validation error.
  /// @endDocuBlock
  ////////////////////////////////////////////////////////////////////////////////

    this.errors = {};

    if (this.schema) {
      if (this.schema.isJoi) {
        this.schema = _.object(_.map(this.schema._inner.children, function (prop) {
          return [prop.key, prop.schema];
        }));
      }
      _.each(
        _.union(_.keys(this.schema), _.keys(attributes)),
        function (key) {
          this.set(key, attributes && attributes[key]);
        },
        this
      );
    } else if (attributes) {
      this.attributes = _.clone(attributes);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_model_get
///
/// `FoxxModel#get(name)`
///
/// Get the value of an attribute
///
/// @EXAMPLES
///
/// ```js
/// instance = new Model({
///   a: 1
/// });
///
/// instance.get("a");
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  get(attributeName) {
    return this.attributes[attributeName];
  }

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_model_set
///
/// `FoxxModel#set(name, value)`
///
/// Set the value of an attribute or multiple attributes at once
///
/// @EXAMPLES
///
/// ```js
/// instance = new Model({
///   a: 1
/// });
///
/// instance.set("a", 2);
/// instance.set({
///   b: 2
/// });
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  set(attributeName, value) {

    if (is.object(attributeName)) {
      _.each(attributeName, function (value, key) {
        this.set(key, value);
      }, this);
      return this;
    }

    if (this.schema) {
      var schema = this.schema[attributeName] || metadataSchema[attributeName] || joi.forbidden();
      var result = schema.validate(value);

      if (result.error) {
        this.errors[attributeName] = result.error;
        this.isValid = false;
      } else {
        delete this.errors[attributeName];
        if (_.isEmpty(this.errors)) {
          this.isValid = true;
        }
      }

      if (result.value === undefined) {
        delete this.attributes[attributeName];
      } else {
        this.attributes[attributeName] = result.value;
      }
    } else {
      this.attributes[attributeName] = value;
    }

    return this;
  }

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_model_has
///
/// `FoxxModel#has(name)`
///
/// Returns true if the attribute is set to a non-null or non-undefined value.
///
/// @EXAMPLES
///
/// ```js
/// instance = new Model({
///   a: 1
/// });
///
/// instance.has("a"); //=> true
/// instance.has("b"); //=> false
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  has(attributeName) {
    return !(_.isUndefined(this.attributes[attributeName]) ||
             _.isNull(this.attributes[attributeName]));
  }

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_model_forDB
///
/// `FoxxModel#forDB()`
///
/// Return a copy of the model which can be saved into ArangoDB
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  forDB() {
    return this.attributes;
  }

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_model_forClient
///
/// `FoxxModel#forClient()`
///
/// Return a copy of the model which you can send to the client.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  forClient() {
    return excludeExtraAttributes(this.attributes, this);
  }
}

Model.fromClient = function (attributes) {
  var model = new this();
  model.set(excludeExtraAttributes(attributes, model));
  return model;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_model_extend
///
/// `FoxxModel#extend(instanceProperties, classProperties)`
///
/// Extend the Model prototype to add or overwrite methods.
/// The first object contains the properties to be defined on the instance,
/// the second object those to be defined on the prototype.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Model.extend = extend;

exports.Model = Model;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:

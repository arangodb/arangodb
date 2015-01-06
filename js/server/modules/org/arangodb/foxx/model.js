/*global require, exports */

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

var Model,
  _ = require("underscore"),
  joi = require("joi"),
  is = require("org/arangodb/is"),
  extend = require('org/arangodb/extend').extend,
  EventEmitter = require('events').EventEmitter,
  util = require('util'),
  excludeExtraAttributes,
  metadataSchema = {
    _id: joi.string().optional(),
    _key: joi.string().optional(),
    _rev: joi.string().optional()
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

excludeExtraAttributes = function (attributes, Model) {
  'use strict';
  var extraAttributeNames;
  if (Model.prototype.schema) {
    extraAttributeNames = _.difference(
      _.keys(metadataSchema),
      _.keys(Model.prototype.schema)
    );
  }
  return _.omit(attributes, extraAttributeNames);
};

Model = function (attributes) {
  'use strict';
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

  var instance = this;
  if (instance.schema) {
    _.each(
      _.union(_.keys(instance.schema), _.keys(attributes)),
      function (attributeName) {
        instance.set(attributeName, attributes ? attributes[attributeName] : undefined);
      }
    );
  } else if (attributes) {
    instance.attributes = _.clone(attributes);
  }
  EventEmitter.call(instance);
};

util.inherits(Model, EventEmitter);

Model.fromClient = function (attributes) {
  'use strict';
  return new this(excludeExtraAttributes(attributes, this));
};

// "Class" Properties
_.extend(Model, {
  // TODO: Docs

  toJSONSchema: function (id) {
    'use strict';
    var required = [],
      properties = {};

    if (this.prototype.schema) {
      _.each(this.prototype.schema, function (schema, attributeName) {
        var description = schema.describe(),
          jsonSchema = {type: description.type},
          rules = description.rules,
          flags = description.flags;

        if (flags && flags.presence === 'required') {
          jsonSchema.required = true;
          required.push(attributeName);
        }

        if (
          jsonSchema.type === 'number' &&
            _.isArray(rules) &&
            _.some(rules, function (rule) {
              return rule.name === 'integer';
            })
        ) {
          jsonSchema.type = 'integer';
        }

        properties[attributeName] = jsonSchema;
      });
    }

    return {
      id: id,
      required: required,
      properties: properties
    };
  }
});

// Instance Properties
_.extend(Model.prototype, {
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

  get: function (attributeName) {
    'use strict';
    return this.attributes[attributeName];
  },

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

  set: function (attributeName, value) {
    'use strict';

    if (is.object(attributeName)) {
      _.each(attributeName, function (value, key) {
        this.set(key, value);
      }, this);
      return;
    }

    if (this.schema) {
      var schema = (
          this.schema[attributeName] ||
          metadataSchema[attributeName] ||
          joi.forbidden()
        ),
        result = (
          schema.isJoi ? schema : joi.object().keys(schema)
        ).validate(value);

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
  },

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

  has: function (attributeName) {
    'use strict';
    return !(_.isUndefined(this.attributes[attributeName]) ||
             _.isNull(this.attributes[attributeName]));
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_model_forDB
///
/// `FoxxModel#forDB()`
///
/// Return a copy of the model which can be saved into ArangoDB
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  forDB: function () {
    'use strict';
    return this.attributes;
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_model_forClient
///
/// `FoxxModel#forClient()`
///
/// Return a copy of the model which you can send to the client.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  forClient: function () {
    'use strict';
    return excludeExtraAttributes(this.attributes, this.constructor);
  }
});

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

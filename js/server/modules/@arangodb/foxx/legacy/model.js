'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2013-2014 triAGENS GmbH, Cologne, Germany
// / Copyright 2015 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Lucas Dohmen
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const joi = require('joi');
const is = require('@arangodb/is');
const toJSONSchema = require('@arangodb/foxx/legacy/schema').toJSONSchema;
const extend = require('@arangodb/extend').extend;
const EventEmitter = require('events').EventEmitter;
const metadataSchema = {
  _id: joi.string().optional(),
  _key: joi.string().optional(),
  _oldRev: joi.string().optional(),
  _rev: joi.string().optional(),
  _from: joi.string().optional(),
  _to: joi.string().optional()
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_foxx_model_initializer
// //////////////////////////////////////////////////////////////////////////////

function excludeExtraAttributes (attributes, model) {
  if (!model.schema) {
    return _.clone(attributes);
  }
  return _.omit(attributes, _.difference(
    _.keys(metadataSchema),
    _.keys(model.schema)
  ));
}

function Model (attributes) {

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_model_attributes
  // //////////////////////////////////////////////////////////////////////////////

  this.attributes = {};

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_model_isvalid
  // //////////////////////////////////////////////////////////////////////////////

  this.isValid = true;

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_model_errors
  // //////////////////////////////////////////////////////////////////////////////

  this.errors = {};

  if (this.schema) {
    if (this.schema.isJoi) {
      this.schema = _.fromPairs(_.map(this.schema._inner.children, function (prop) {
        return [prop.key, prop.schema];
      }));
    }
    _.each(
      _.union(_.keys(this.schema), _.keys(attributes)),
      function (key) {
        this.set(key, attributes && attributes[key]);
      }.bind(this)
    );
  } else if (attributes) {
    this.attributes = _.clone(attributes);
  }
}

Model.prototype = Object.create(EventEmitter.prototype);
Object.assign(Model.prototype, {

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_model_get
  // //////////////////////////////////////////////////////////////////////////////

  get(attributeName) {
    return this.attributes[attributeName];
  },

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_model_set
  // //////////////////////////////////////////////////////////////////////////////

  set(attributeName, value) {
    if (is.object(attributeName)) {
      _.each(attributeName, function (value, key) {
        this.set(key, value);
      }.bind(this));
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
  },

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_model_has
  // //////////////////////////////////////////////////////////////////////////////

  has(attributeName) {
    return !(_.isUndefined(this.attributes[attributeName]) ||
    _.isNull(this.attributes[attributeName]));
  },

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_model_forDB
  // //////////////////////////////////////////////////////////////////////////////

  forDB() {
    return this.attributes;
  },

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief was docuBlock JSF_foxx_model_forClient
  // //////////////////////////////////////////////////////////////////////////////

  forClient() {
    return excludeExtraAttributes(this.attributes, this);
  }
});

Model.toJSONSchema = function (id) {
  return toJSONSchema(id, this);
};

Model.fromClient = function (attributes) {
  var model = new this();
  model.set(excludeExtraAttributes(attributes, model));
  return model;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_foxx_model_extend
// //////////////////////////////////////////////////////////////////////////////

Model.extend = extend;

exports.Model = Model;

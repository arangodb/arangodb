/*global exports, require */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx schema helper
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 triAGENS GmbH, Cologne, Germany
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
/// @author Alan Plum
/// @author Copyright 2015, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var _ = require('underscore');

function toJSONSchema(id, schema) {
  'use strict';
  var required = [],
    properties = {};


  if (schema) {
    if (
      typeof schema.toJSONSchema === 'function' &&
      schema.toJSONSchema !== toJSONSchema &&
      schema.toJSONSchema !== require('org/arangodb/foxx/model').Model.toJSONSchema
    ) {
      // allow overriding toJSONSchema.
      return schema.toJSONSchema(id);
    }
    if (!schema.isJoi) {
      // assume schema is a Model
      schema = schema.prototype.schema;
    }
    _.each(schema, function (attributeSchema, attributeName) {
      var description = attributeSchema.describe(),
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

exports.toJSONSchema = toJSONSchema;
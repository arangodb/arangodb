'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
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
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

var _ = require('lodash'),
  joi = require('joi');

function toJSONSchema (id, schema) {
  var jsonSchema = {
    id: id,
    required: [],
    properties: {}
  };

  if (!schema) {
    return jsonSchema;
  }

  if (
    typeof schema.toJSONSchema === 'function' &&
    schema.toJSONSchema !== toJSONSchema &&
    schema.toJSONSchema !== require('@arangodb/foxx/legacy/model').Model.toJSONSchema
  ) {
    // allow overriding toJSONSchema.
    return schema.toJSONSchema(id);
  }

  if (typeof schema === 'function') {
    // assume schema is a Model
    schema = schema.prototype.schema || {};
  }

  if (!schema.isJoi) {
    schema = joi.object().keys(schema);
  }

  var description = schema.describe();

  if (description.type !== 'object') {
    return jsonSchema;
  }

  _.each(description.children, function (attributeDescription, attributeName) {
    var attributeSchema = {type: attributeDescription.type},
      rules = attributeDescription.rules,
      flags = attributeDescription.flags;

    if (flags && flags.presence === 'required') {
      jsonSchema.required.push(attributeName);
    }

    if (
      attributeSchema.type === 'number' &&
      _.isArray(rules) &&
      _.some(rules, function (rule) {
        return rule.name === 'integer';
      })
    ) {
      attributeSchema.type = 'integer';
    }

    jsonSchema.properties[attributeName] = attributeSchema;
  });

  if (!jsonSchema.required.length) {
    delete jsonSchema.required;
  }

  return jsonSchema;
}

exports.toJSONSchema = toJSONSchema;

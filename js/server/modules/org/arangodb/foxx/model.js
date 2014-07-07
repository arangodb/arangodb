/*jslint indent: 2, nomen: true, maxlen: 120 */
/*global module, require, exports */

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
  is = require("org/arangodb/is"),
  extend = require('org/arangodb/extend').extend,
  metadataKeys = ['_id', '_key', '_rev'],
  getTypeFromJoi,
  getTypeFromString,
  getTypeFromSchema,
  isRequiredByJoi,
  isRequiredBySchema,
  parseAttributes,
  parseRequiredAttributes,
  validateJoi,
  validateSchema;

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

var whitelistProperties = function (properties, constructorProperties, whitelistMetadata) {
  'use strict';
  if (!properties) {
    return {};
  }

  if (!constructorProperties) {
    return _.clone(properties);
  }

  var whitelistedKeys = _.keys(constructorProperties);

  if (whitelistMetadata) {
    whitelistedKeys = whitelistedKeys.concat(metadataKeys);
  }

  return _.pick(properties, whitelistedKeys);
};

var fillInDefaults = function (properties, constructorProperties) {
  'use strict';
  if (!constructorProperties) {
    return properties;
  }
  var defaults = _.reduce(constructorProperties, function (result, value, key) {
    if (_.has(value, "defaultValue")) {
      result[key] = value.defaultValue;
    }

    return result;
  }, {});

  return _.defaults(properties, defaults);
};

Model = function (attributes) {
  'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_foxx_model_attributes
/// @brief The attributes property is the internal hash containing the model's state.
////////////////////////////////////////////////////////////////////////////////

  this.attributes = whitelistProperties(attributes, this.constructor.attributes, true);
  this.attributes = fillInDefaults(this.attributes, this.constructor.attributes);
  this.whitelistedAttributes = whitelistProperties(this.attributes, this.constructor.attributes);
};

Model.fromClient = function (attributes) {
  'use strict';

  var instance = new this(attributes);
  instance.attributes = whitelistProperties(attributes, this.attributes, false);
  instance.attributes = fillInDefaults(instance.attributes, this.attributes);
  instance.whitelistedAttributes = whitelistProperties(instance.attributes, this.attributes);
  return instance;
};

getTypeFromJoi = function (value) {
  'use strict';
  if (!value || typeof value.describe !== 'function') {
    return undefined;
  }

  var description = value.describe(),
    rules = description.rules;

  if (
    description.type === 'number' &&
      _.isArray(rules) &&
      _.some(rules, function (rule) {
        return rule.name === 'integer';
      })
  ) {
    return 'integer';
  }
  return description.type;
};

getTypeFromString = function (value) {
  'use strict';
  if (!_.isString(value)) {
    return undefined;
  }
  return value;
};

getTypeFromSchema = function (value) {
  'use strict';
  if (!value && !value.type) {
    return undefined;
  }
  return value.type;
};

isRequiredByJoi = function (value) {
  'use strict';
  if (!value || typeof value.describe !== 'function') {
    return false;
  }
  var description = value.describe();
  return Boolean(
    description.flags &&
      description.flags.presence === 'required'
  );
};

isRequiredBySchema = function (value) {
  'use strict';
  return Boolean(value && value.required);
};

parseAttributes = function (rawAttributes) {
  'use strict';
  var properties = {};

  _.each(rawAttributes, function (value, key) {
    var type = getTypeFromJoi(value) || getTypeFromString(value) || getTypeFromSchema(value);
    if (type) {
      properties[key] = {type: type};
    }
  });

  return properties;
};

parseRequiredAttributes = function (rawAttributes) {
  'use strict';
  var requiredProperties = [];

  _.each(rawAttributes, function (value, key) {
    if (isRequiredByJoi(value) || isRequiredBySchema(value)) {
      requiredProperties.push(key);
    }
  });

  return requiredProperties;
};

validateJoi = function (key, value, joi) {
  'use strict';
  if (!joi || typeof joi.validate !== "function") {
    return;
  }
  var result = joi.validate(value);
  if (result.error) {
    result.error.message = "Invalid value for \"" + key + "\": " + result.error.message;
    throw result.error;
  }
  return result.value;
};

validateSchema = function (key, value, schema) {
  'use strict';
  if (!schema) {
    return;
  }

  var actualType = typeof value,
    expectedType = typeof schema === "string" ? schema : schema.type;

  if (typeof expectedType !== "string") {
    return;
  }

  if (_.isArray(value)) {
    actualType = "array";
  }

  if (expectedType === "integer" && actualType === "number") {
    actualType = (Math.floor(value) === value ? "integer" : "number");
  }

  if (actualType !== expectedType) {
    throw new Error(
      "Invalid value for \"" +
        key + "\": Expected " + expectedType +
        " instead of " + actualType + "!"
    );
  }
};

// "Class" Properties
_.extend(Model, {
  // TODO: Docs

  toJSONSchema: function (id) {
    'use strict';
    var attributes = this.attributes;

    return {
      id: id,
      required: parseRequiredAttributes(attributes),
      properties: parseAttributes(attributes)
    };
  }
});

// Instance Properties
_.extend(Model.prototype, {
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_model_get
///
/// `FoxxModel::get(name)`
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
/// `FoxxModel::set(name, value)`
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
    var constructorAttributes = this.constructor.attributes,
      attributes = attributeName;

    if (!is.object(attributeName)) {
      attributes = {};
      attributes[attributeName] = value;
    }

    if (constructorAttributes) {
      _.each(_.keys(attributes), function (key) {
        if (!constructorAttributes.hasOwnProperty(key)) {
          if (metadataKeys.indexOf(key) !== -1) {
            return;
          }
          throw new Error("Unknown attribute: " + key);
        }

        var value = attributes[key],
          result;

        if (value === undefined || value === null) {
          return;
        }

        result = validateJoi(key, value, constructorAttributes[key]);
        validateSchema(key, value, constructorAttributes[key]);

        if (result) {
          attributes[key] = result;
        }
      });
    }

    _.extend(this.attributes, attributes);
    this.whitelistedAttributes = whitelistProperties(this.attributes, constructorAttributes);
  },

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_model_has
///
/// `FoxxModel::has(name)`
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
/// `FoxxModel::forDB()`
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
/// `FoxxModel::forClient()`
///
/// Return a copy of the model which you can send to the client.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

  forClient: function () {
    'use strict';
    return this.whitelistedAttributes;
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_foxx_model_extend
///
/// `FoxxModel::extend(instanceProperties, classProperties)`
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

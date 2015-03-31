/*global require, assertFalse, assertTrue, assertEqual */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief test the agency communication layer
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Lucas Doomen
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var _ = require("underscore"),
  joi = require("joi"),
  jsunity = require("jsunity");

function ModelSpec () {
  var FoxxModel, instance;

  return {
    setUp: function () {
      FoxxModel = require('org/arangodb/foxx/model').Model;
    },

    testWithInitialData: function () {
      instance = new FoxxModel({
        a: 1
      });

      assertEqual(instance.get("a"), 1);
      instance.set("a", 2);
      assertEqual(instance.get("a"), 2);
    },

    testWithoutInitialData: function () {
      instance = new FoxxModel();

      assertEqual(instance.get("a"), undefined);
      instance.set("a", 1);
      assertEqual(instance.get("a"), 1);
    },

    testFromDb: function () {
      require("internal").wal.flush(true, true);
      var doc = require("org/arangodb").db._users.any();
      assertEqual(typeof doc._PRINT, 'function');
      instance = new FoxxModel(doc);
      assertEqual(instance.attributes._PRINT, undefined);
      assertFalse(instance.has('_PRINT'));
    },

    testFromDbWithSchema: function () {
      var Model = FoxxModel.extend({
        schema: {
          user: joi.string()
        }
      });
      require("internal").wal.flush(true, true);
      var doc = require("org/arangodb").db._users.any();
      assertEqual(typeof doc._PRINT, 'function');
      instance = new Model(doc);
      assertEqual(instance.attributes._PRINT, undefined);
      assertFalse(instance.has('_PRINT'));
    },

    testSettingFromDb: function () {
      require("internal").wal.flush(true, true);
      var doc = require("org/arangodb").db._users.any();
      assertEqual(typeof doc._PRINT, 'function');
      instance = new FoxxModel();
      instance.set(doc);
      assertEqual(instance.attributes._PRINT, undefined);
      assertFalse(instance.has('_PRINT'));
    },

    testSettingFromDbWithSchema: function () {
      var Model = FoxxModel.extend({
        schema: {
          user: joi.string()
        }
      });
      require("internal").wal.flush(true, true);
      var doc = require("org/arangodb").db._users.any();
      assertEqual(typeof doc._PRINT, 'function');
      instance = new Model();
      instance.set(doc);
      assertEqual(instance.attributes._PRINT, undefined);
      assertFalse(instance.has('_PRINT'));
    },

    testSettingMultipleAttributes: function () {
      instance = new FoxxModel({
        a: 1,
        b: 9
      });

      instance.set({
        b: 2,
        c: 3
      });

      assertEqual(instance.get("a"), 1);
      assertEqual(instance.get("b"), 2);
      assertEqual(instance.get("c"), 3);
    },

    testAddingAMethodWithExtend: function () {
      var Model = FoxxModel.extend({
        getA: function() {
          return this.get("a");
        }
      });

      instance = new Model({
        a: 5
      });

      assertEqual(instance.getA(), 5);
    },

    testOverwritingAMethodWithExtend: function () {
      var Model = FoxxModel.extend({
        get: function() {
          return 1;
        }
      });

      instance = new Model({
        a: 5
      });

      assertEqual(instance.get(), 1);
    },

    testHas: function () {
      instance = new FoxxModel({
        a: 1,
        b: null
      });

      assertTrue(instance.has("a"));
      assertFalse(instance.has("b"));
      assertFalse(instance.has("c"));
    },

    testSerialization: function () {
      var raw = {
        a: 1,
        b: 2
      };

      instance = new FoxxModel(raw);
      var dbData = instance.forDB();
      var clientData = instance.forClient();

      _.each(raw, function(value, key) {
        assertEqual(value, dbData[key]);
        assertEqual(value, clientData[key]);
      });
    },

    testAlwaysAllowMetadataAttributes: function () {
      var Model = FoxxModel.extend({
        schema: {
          anInteger: joi.number().integer()
        }
      });

      instance = new Model({ anInteger: 1 });
      assertEqual(_.keys(instance.attributes).length, 1);
      instance.set("_key", "arango");
      assertEqual(_.keys(instance.attributes).length, 2);
      assertEqual(instance.get("_key"), "arango");
      assertEqual(_.keys(instance.errors).length, 0);
      assertTrue(instance.isValid);
    },

    testCoerceAttributes: function () {
      var Model = FoxxModel.extend({
        schema: {
          anInteger: joi.number().integer()
        }
      });

      instance = new Model({anInteger: 1});
      instance.set("anInteger", "2");
      assertEqual(instance.get("anInteger"), 2);
      assertEqual(_.keys(instance.errors).length, 0);
      assertTrue(instance.isValid);
    },

    testDisallowUnknownAttributes: function () {
      var Model = FoxxModel.extend({
        schema: {
          anInteger: joi.number().integer().strict()
        }
      });

      instance = new Model({ anInteger: 1, nonExistant: 2 });
      assertEqual(instance.attributes.anInteger, 1);
      assertEqual(instance.attributes.nonExistant, 2);
      assertEqual(_.keys(instance.errors).length, 1);
      assertFalse(instance.isValid);
      assertEqual(instance.errors.nonExistant._object, 2);
      assertTrue(instance.errors.nonExistant.message.indexOf("not allowed") !== -1);

      instance.set('nonExistant', undefined);
      assertEqual(_.keys(instance.errors).length, 0);
      assertTrue(instance.isValid);
    },

    testDisallowInvalidAttributes: function () {
      var Model = FoxxModel.extend({
        schema: {
          anInteger: joi.number().integer().strict()
        }
      });

      instance = new Model({ anInteger: 1 });
      instance.set("anInteger", undefined);
      assertEqual(instance.attributes.anInteger, undefined);
      instance.set("anInteger", 2);
      assertEqual(instance.attributes.anInteger, 2);
      assertEqual(_.keys(instance.errors).length, 0);
      assertTrue(instance.isValid);

      instance.set("anInteger", 5.1);
      assertEqual(instance.attributes.anInteger, 5.1);
      assertEqual(_.keys(instance.errors).length, 1);
      assertFalse(instance.isValid);
      assertEqual(instance.errors.anInteger._object, 5.1);
      assertTrue(instance.errors.anInteger.message.indexOf("integer") !== -1);

      instance.set("anInteger", false);
      assertEqual(instance.attributes.anInteger, false);
      assertEqual(_.keys(instance.errors).length, 1);
      assertFalse(instance.isValid);
      assertEqual(instance.errors.anInteger._object, false);
      assertTrue(instance.errors.anInteger.message.indexOf("number") !== -1);

      instance.set("anInteger", "");
      assertEqual(instance.attributes.anInteger, "");
      assertEqual(_.keys(instance.errors).length, 1);
      assertFalse(instance.isValid);
      assertEqual(instance.errors.anInteger._object, "");
      assertTrue(instance.errors.anInteger.message.indexOf("number") !== -1);

      instance.set("anInteger", 4);
      assertEqual(instance.attributes.anInteger, 4);
      assertEqual(_.keys(instance.errors).length, 0);
      assertTrue(instance.isValid);
    },

    testFilterMetadata: function () {
      var Model = FoxxModel.extend({
        schema: {
          a: joi.number().integer()
        }
      });

      var raw = {
        a: 1,
        _key: 2
      };

      instance = new Model(raw);
      var dbData = instance.forDB();
      var clientData = instance.forClient();

      assertEqual(_.keys(dbData).length, 2);
      assertEqual(_.keys(clientData).length, 1);
      assertEqual(dbData.a, raw.a);
      assertEqual(dbData._key, raw._key);
      assertEqual(clientData.a, raw.a);
    },

    testFromClient: function () {
      var Model = FoxxModel.extend({
        schema: {
          a: joi.number().integer()
        }
      });

      var raw = {
        a: 1,
        _key: 2
      };

      instance = Model.fromClient(raw);
      assertEqual(_.keys(instance.attributes).length, 1);
      assertEqual(instance.attributes.a, raw.a);
      assertEqual(instance.attributes._key, undefined);
    },

    testSetDefaultAttributes: function () {
      var Model = FoxxModel.extend({
        schema: {
          a: joi.string().default('test'),
          b: joi.string().default('test'),
          c: joi.string()
        }
      });

      instance = new Model({ a: "a", c: "c" });
      assertEqual(instance.attributes, { a: "a", c: "c", b: "test" });
    }
  };
}

function ModelAnnotationSpec () {
  var FoxxModel, toJSONSchema, jsonSchema, instance;

  return {
    setUp: function () {
      FoxxModel = require('org/arangodb/foxx/model').Model;
      toJSONSchema = require('org/arangodb/foxx/schema').toJSONSchema;
    },

    testGetEmptyJSONSchema: function () {
      var Model = FoxxModel.extend({});
      jsonSchema = toJSONSchema("myname", Model);
      assertEqual(jsonSchema.id, "myname");
      assertEqual(jsonSchema.required, []);
      assertEqual(jsonSchema.properties, {});
    },

    testAttributesOfAPlainModel: function () {
      var attributes = {a: 1, b: 2};
      var Model = FoxxModel.extend({});
      instance = new Model(attributes);
      assertEqual(instance.attributes, attributes);
    },

    testAddOptionalAttributeToJSONSchema: function () {
      var Model = FoxxModel.extend({
        schema: {
          x: joi.string()
        }
      });

      jsonSchema = toJSONSchema("myname", Model);
      assertEqual(jsonSchema.id, "myname");
      assertEqual(jsonSchema.required, []);
      assertEqual(jsonSchema.properties.x.type, "string");
    },

    testAddRequiredAttributeToJSONSchema: function () {
      var Model = FoxxModel.extend({
        schema: {
          x: joi.string().required()
        }
      });

      jsonSchema = toJSONSchema("myname", Model);
      assertEqual(jsonSchema.id, "myname");
      assertEqual(jsonSchema.properties.x.type, "string");
      assertEqual(jsonSchema.required, ["x"]);
    }
  };
}

jsunity.run(ModelSpec);
jsunity.run(ModelAnnotationSpec);

return jsunity.done();

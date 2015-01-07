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

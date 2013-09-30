require("internal").flushModuleCache();

var jsunity = require("jsunity");

function ModelSpec () {
  var FoxxModel, TestModel, instance;

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
      TestModel = FoxxModel.extend({
        getA: function() {
          return this.get("a");
        }
      });

      instance = new TestModel({
        a: 5
      });

      assertEqual(instance.getA(), 5);
    },

    testOverwritingAMethodWithExtend: function () {
      TestModel = FoxxModel.extend({
        get: function() {
          return 1;
        }
      });

      instance = new TestModel({
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

      assertEqual(instance.forDB(), raw);
      assertEqual(instance.forClient(), raw);
    }
  };
}

function ModelAnnotationSpec () {
  var FoxxModel, TestModel, jsonSchema, instance;

  return {
    setUp: function () {
      FoxxModel = require('org/arangodb/foxx/model').Model;
    },

    testGetEmptyJSONSchema: function () {
      TestModel = FoxxModel.extend({});
      jsonSchema = TestModel.toJSONSchema("myname");
      assertEqual(jsonSchema.id, "myname");
      assertEqual(jsonSchema.required, []);
      assertEqual(jsonSchema.properties, {});
    },

    testAttributesOfAPlainModel: function () {
      attributes = {a: 1, b: 2};
      TestModel = FoxxModel.extend({});
      instance = new TestModel(attributes);
      assertEqual(instance.attributes, attributes);
    },

    testAddOptionalAttributeToJSONSchemaInLongForm: function () {
      TestModel = FoxxModel.extend({}, {
        attributes: {
          x: { type: "string" }
        }
      });

      jsonSchema = TestModel.toJSONSchema("myname");
      assertEqual(jsonSchema.id, "myname");
      assertEqual(jsonSchema.required, []);
      assertEqual(jsonSchema.properties.x.type, "string");
    },

    testAddOptionalAttributeToJSONSchemaInShortForm: function () {
      TestModel = FoxxModel.extend({}, {
        attributes: {
          x: "string"
        }
      });

      jsonSchema = TestModel.toJSONSchema("myname");
      assertEqual(jsonSchema.id, "myname");
      assertEqual(jsonSchema.required, []);
      assertEqual(jsonSchema.properties.x.type, "string");
    },

    testAddRequiredAttributeToJSONSchema: function () {
      TestModel = FoxxModel.extend({}, {
        attributes: {
          x: { type: "string", required: true }
        }
      });

      jsonSchema = TestModel.toJSONSchema("myname");
      assertEqual(jsonSchema.id, "myname");
      assertEqual(jsonSchema.properties.x.type, "string");
      assertEqual(jsonSchema.required, ["x"]);
    },

    testWhitelistConstructorAttributesInAnnotatedModel: function () {
      TestModel = FoxxModel.extend({}, {
        attributes: {
          a: { type: "string", required: true },
          b: { type: "string" }
        }
      });

      instance = new TestModel({ a: "a", b: "b", c: "c" });
      assertEqual(instance.attributes, { a: "a", b: "b" });
    },

    testSetDefaultAttributesInAnnotatedModel: function () {
      TestModel = FoxxModel.extend({}, {
        attributes: {
          a: { type: "string", defaultValue: "test" },
          b: { type: "string", defaultValue: "test" },
          c: { type: "string" }
        }
      });

      instance = new TestModel({ a: "a", c: "c" });
      assertEqual(instance.attributes, { a: "a", c: "c", b: "test" });
    }
  };
}

jsunity.run(ModelSpec);
jsunity.run(ModelAnnotationSpec);

return jsunity.done();

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

jsunity.run(ModelSpec);

return jsunity.done();

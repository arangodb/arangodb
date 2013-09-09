require("internal").flushModuleCache();

var jsunity = require("jsunity"),
  FoxxRepository = require("org/arangodb/foxx/repository").Repository,
  Model = require("org/arangodb/foxx/model").Model;

function RepositorySpec () {
  var TestRepository, instance, prefix, collection, modelPrototype;

  return {
    setUp: function () {
      prefix = "myApp";
      collection = function () {};
      modelPrototype = function () {};
    },

    testInitializeWithCollectionOnly: function () {
      instance = new FoxxRepository(collection);

      assertEqual(instance.collection, collection);
      assertEqual(instance.modelPrototype, Model);
    },

    testInitializeWithCollectionAndModelPrototype: function () {
      instance = new FoxxRepository(collection, {
        model: modelPrototype
      });

      assertEqual(instance.collection, collection);
      assertEqual(instance.modelPrototype, modelPrototype);
    },

    testInitializeWithCollectionAndPrefix: function () {
      instance = new FoxxRepository(collection, {
        prefix: prefix
      });

      assertEqual(instance.collection, collection);
      assertEqual(instance.modelPrototype, Model);
      assertEqual(instance.prefix, prefix);
    },

    testAddingAMethodWithExtend: function () {
      TestRepository = FoxxRepository.extend({
        test: function() {
          return "test";
        }
      });

      instance = new TestRepository(collection);

      assertEqual(instance.test(), "test");
    },

    testDelegatesToCollection: function () {
      var arg = function () {};

      [
        "save",
        "remove",
        "replace",
        "update",
        "removeByExample",
        "replaceByExample",
        "updateByExample",
        "all",
        "byExample",
        "firstExample"
      ].forEach(function (f) {
        var called = false;
        collection = function () {};
        collection[f] = function (x) {
          called = (arg === x);
        };
        instance = new FoxxRepository(collection);
        instance[f](arg);
        assertTrue(called);
      });
    }
  };
}

jsunity.run(RepositorySpec);

return jsunity.done();

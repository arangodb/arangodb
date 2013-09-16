require("internal").flushModuleCache();

var jsunity = require("jsunity"),
  FoxxRepository = require("org/arangodb/foxx/repository").Repository,
  Model = require("org/arangodb/foxx/model").Model;


function RepositorySpec () {
  var TestRepository, instance, prefix, collection, modelPrototype, model, modelData;

  return {
    setUp: function () {
      prefix = "myApp";
      collection = function () {};
      modelPrototype = function () {};
      model = new modelPrototype();
      modelData = { fancy: 1 };
      model.forDB = function () {
        return modelData;
      };
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

    testNothing: function () {
      [
        "save",

        "removeById",
        "removeByExample",

        "replaceById",
        "replaceByExample",

        "updateById",
        "updateByExample",

        "byId",
        "byExample",
        "firstExample"
      ].forEach(function (f) {
        instance = new FoxxRepository(collection);
        assertTrue(instance[f] !== undefined);
      });
    }
  };
}

function RepositoryMethodsSpec() {
  var instance,
    collection,
    ModelPrototype,
    model,
    modelData,
    id,
    id_and_rev;

  return {
    setUp: function () {
      // Stubs:
      // Basically, this should be `let(:x) { double }`
      ModelPrototype = function () {};
      id_and_rev = function () {};
      model = function () {};
      modelData = function () {};
      id = function () {};

      // Stubbed Functions:
      // Basically, this should be `allow(x).to receive(:forDB).and_return(y)`
      model.forDB = function () {
        return modelData;
      };
    },

    testSave: function () {
      var called = false;

      model.set = function (x) {
        called = (x === id_and_rev);
      };

      collection = {
        save: function(x) {
          if (x === modelData) {
            return id_and_rev;
          }
        }
      };

      instance = new FoxxRepository(collection, { model: ModelPrototype });
      instance.save(model);
      assertTrue(called);
    },

    testRemoveById: function () {
      var called = false;

      collection = {
        remove: function(x) {
          called = (x === id);
        }
      };

      instance = new FoxxRepository(collection, { model: ModelPrototype });
      instance.removeById(id);
      assertTrue(called);
    }
  };
}

jsunity.run(RepositorySpec);
jsunity.run(RepositoryMethodsSpec);

return jsunity.done();

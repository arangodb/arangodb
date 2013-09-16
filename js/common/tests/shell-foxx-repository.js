require("internal").flushModuleCache();

var jsunity = require("jsunity"),
  FoxxRepository = require("org/arangodb/foxx/repository").Repository,
  Model = require("org/arangodb/foxx/model").Model,
  stub_and_mock = require("org/arangodb/stub_and_mock"),
  stub = stub_and_mock.stub,
  allow = stub_and_mock.allow,
  expect = stub_and_mock.expect;

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
      ModelPrototype = stub();
      id_and_rev = stub();
      modelData = stub();
      model = stub();
      collection = stub();
      id = stub();

      allow(model)
        .toReceive("forDB")
        .andReturn(modelData);

      instance = new FoxxRepository(collection, { model: ModelPrototype });
    },

    testSave: function () {
      expect(model)
        .toReceive("set")
        .withArguments([ id_and_rev ]);

      expect(collection)
        .toReceive("save")
        .withArguments([ modelData ])
        .andReturn(id_and_rev);

      instance.save(model);

      model.assertIsSatisfied();
      collection.assertIsSatisfied();
    },

    testRemoveById: function () {
      expect(collection)
        .toReceive("remove")
        .withArguments([ id ]);

      instance.removeById(id);

      collection.assertIsSatisfied();
    }
  };
}

jsunity.run(RepositorySpec);
jsunity.run(RepositoryMethodsSpec);

return jsunity.done();

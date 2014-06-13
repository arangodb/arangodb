/*jslint indent: 2, nomen: true, maxlen: 120 */
/*global module, require, exports, assertEqual, assertTrue, assertIsSatisfied */

require("internal").flushModuleCache();

var jsunity = require("jsunity"),
  FoxxRepository = require("org/arangodb/foxx/repository").Repository,
  Model = require("org/arangodb/foxx/model").Model,
  stub_and_mock = require("org/arangodb/stub_and_mock"),
  stub = stub_and_mock.stub,
  allow = stub_and_mock.allow,
  expect = stub_and_mock.expect,
  mockConstructor = stub_and_mock.mockConstructor;

function RepositorySpec () {
  var TestRepository, instance, prefix, collection, modelPrototype, model, modelData;

  return {
    setUp: function () {
      prefix = "myApp";
      collection = stub();
      modelPrototype = stub();
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
    }
  };
}

function RepositoryMethodsSpec() {
  var instance,
    collection,
    ModelPrototype,
    model,
    modelData,
    data,
    example,
    cursor,
    id,
    id_and_rev;

  return {
    setUp: function () {
      ModelPrototype = stub();
      id_and_rev = stub();
      modelData = stub();
      example = stub();
      cursor = stub();
      model = stub();
      data = stub();
      collection = stub();
      id = stub();

      allow(model)
        .toReceive("forDB")
        .andReturn(modelData);

      instance = new FoxxRepository(collection, { model: ModelPrototype });
    },

    // Adding Entries

    testSave: function () {
      expect(model)
        .toReceive("set")
        .withArguments(id_and_rev);

      expect(collection)
        .toReceive("save")
        .withArguments(modelData)
        .andReturn(id_and_rev);

      assertEqual(instance.save(model), model);

      model.assertIsSatisfied();
      collection.assertIsSatisfied();
    },

    // Finding Entries

    testById: function () {
      expect(collection)
        .toReceive("document")
        .withArguments(id)
        .andReturn(data);

      ModelPrototype = mockConstructor(data);
      instance = new FoxxRepository(collection, { model: ModelPrototype });

      model = instance.byId(id);

      assertTrue(model instanceof ModelPrototype);
      ModelPrototype.assertIsSatisfied();
      collection.assertIsSatisfied();
    },

    testByExample: function () {
      expect(collection)
        .toReceive("byExample")
        .withArguments(example)
        .andReturn(cursor);

      allow(cursor)
        .toReceive("toArray")
        .andReturn([data]);

      ModelPrototype = mockConstructor(data);
      instance = new FoxxRepository(collection, { model: ModelPrototype });

      var models = instance.byExample(example);

      assertTrue(models[0] instanceof ModelPrototype);
      ModelPrototype.assertIsSatisfied();
      collection.assertIsSatisfied();
    },

    testFirstExample: function () {
      expect(collection)
        .toReceive("firstExample")
        .withArguments(example)
        .andReturn(data);

      ModelPrototype = mockConstructor(data);
      instance = new FoxxRepository(collection, { model: ModelPrototype });

      model = instance.firstExample(example);

      assertTrue(model instanceof ModelPrototype);
      ModelPrototype.assertIsSatisfied();
      collection.assertIsSatisfied();
    },

    // TODO: Implement
    // testAll: function () {
    // },

    // Removing Entries

    // TODO: Implement
    // testRemove: function () {
    // },

    testRemoveById: function () {
      expect(collection)
        .toReceive("remove")
        .withArguments(id);

      instance.removeById(id);

      collection.assertIsSatisfied();
    },

    testRemoveByExample: function () {
      expect(collection)
        .toReceive("removeByExample")
        .withArguments(example);

      instance.removeByExample(example);

      collection.assertIsSatisfied();
    },

    // Replacing Entries

    testReplace: function () {
      allow(model)
        .toReceive("get")
        .andReturn(id);

      allow(model)
        .toReceive("forDB")
        .andReturn(data);

      expect(model)
        .toReceive("set")
        .withArguments(id_and_rev);

      expect(collection)
        .toReceive("replace")
        .withArguments(id, data)
        .andReturn(id_and_rev);

      assertEqual(instance.replace(model), model);

      collection.assertIsSatisfied();
      model.assertIsSatisfied();
    },

    testReplaceById: function () {
      allow(model)
        .toReceive("forDB")
        .andReturn(data);

      expect(model)
        .toReceive("set")
        .withArguments(id_and_rev);

      expect(collection)
        .toReceive("replace")
        .withArguments(id, data)
        .andReturn(id_and_rev);

      assertEqual(instance.replaceById(id, model), model);

      collection.assertIsSatisfied();
      model.assertIsSatisfied();
    },

    // TODO: Implement
    // testRemoveByExample: function () {
    // },

    // Updating Entries

    // TODO: Implement
    // testUpdateById: function () {
    // },

    // TODO: Implement
    // testUpdateByExample: function () {
    // },

    // Counting Entries

    // TODO: Implement
    // testCount: function () {
    // },
  };
}

jsunity.run(RepositorySpec);
jsunity.run(RepositoryMethodsSpec);

return jsunity.done();

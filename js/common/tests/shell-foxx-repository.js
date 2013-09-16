require("internal").flushModuleCache();

var jsunity = require("jsunity"),
  FoxxRepository = require("org/arangodb/foxx/repository").Repository,
  Model = require("org/arangodb/foxx/model").Model,
  _ = require("underscore"),
  stub,
  allow,
  expect,
  FunctionStub,
  FunctionMock;

/* Mini Stub and Mock Framework
 *
 * Sorry for Yak Shaving. But I can't take it anymore.
 */

// x = stub();

stub = function () {
  return function() {};
};

// allow(x).to({
//   receive: "functionName",
//   and_return: { x: 1 }
// });

FunctionStub = function(obj) {
  this.obj = obj;
};

FunctionStub.prototype.to = function(config) {
  this.obj[config.receive] = function () {
    return config.and_return;
  };
};

allow = function(obj) {
  return (new FunctionStub(obj));
};

// expect(x).to({
//   receive: "functionName",
//   withArguments: [ 5 ],
//   and_return { x: 1 }
// });
//
// ...
//
// x.assertIsSatisfied();

FunctionMock = function(obj) {
  this.obj = obj;
  this.obj.satisfied = true;

  this.obj.assertIsSatisfied = function () {
    assertTrue(this.satisfied, "Mock Expectation was not satisfied");
  };
};

FunctionMock.prototype.to = function(config) {
  var obj = this.obj;
  obj.satisfied = false;

  this.obj[config.receive] = function () {
    var args = Array.prototype.slice.call(arguments, 0);

    if ((config.withArguments === undefined) || (_.isEqual(args, config.withArguments))) {
      obj.satisfied = true;
    }

    return config.and_return;
  };
};

expect = function(obj) {
  return (new FunctionMock(obj));
};

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
      id = stub();
    },

    testSave: function () {
      var called = false;

      model = stub();
      collection = stub();

      allow(model).to({
        receive: "forDB",
        and_return: modelData
      });

      expect(model).to({
        receive: "set",
        withArguments: [ id_and_rev ]
      });

      expect(collection).to({
        receive: "save",
        withArguments: [ modelData ],
        and_return: id_and_rev
      });

      instance = new FoxxRepository(collection, { model: ModelPrototype });
      instance.save(model);

      model.assertIsSatisfied();
      collection.assertIsSatisfied();
    },

    testRemoveById: function () {
      model = stub();
      collection = stub();

      expect(collection).to({
        receive: "remove",
        withArguments: [ id ]
      });

      instance = new FoxxRepository(collection, { model: ModelPrototype });
      instance.removeById(id);
      collection.assertIsSatisfied();
    }
  };
}

jsunity.run(RepositorySpec);
jsunity.run(RepositoryMethodsSpec);

return jsunity.done();

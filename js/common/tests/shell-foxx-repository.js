require("internal").flushModuleCache();

var jsunity = require("jsunity"),
  FoxxRepository = require('org/arangodb/foxx/repository').Repository;

function RepositorySpec () {
  var TestRepository, instance, prefix, collection, modelPrototype;

  return {
    setUp: function () {
      prefix = "myApp";
      collection = function () {};
      modelPrototype = function () {};
    },

    testWasInitialized: function () {
      instance = new FoxxRepository(prefix, collection, modelPrototype);

      assertEqual(instance.prefix, prefix);
      assertEqual(instance.collection, collection);
      assertEqual(instance.modelPrototype, modelPrototype);
    },

    testAddingAMethodWithExtend: function () {
      TestRepository = FoxxRepository.extend({
        test: function() {
          return "test";
        }
      });

      instance = new TestRepository(prefix, collection, modelPrototype);

      assertEqual(instance.test(), "test");
    }
  };
}

jsunity.run(RepositorySpec);

return jsunity.done();

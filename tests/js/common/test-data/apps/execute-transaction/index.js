const createRouter = require('@arangodb/foxx/router');
const router = createRouter();

router.get('/execute', function (req, res) {
  let db = require("@arangodb").db;
  db._executeTransaction({
    collections: { write: "UnitTestsTransaction" },
    action: function() {
      let db = require("@arangodb").db;
      db.UnitTestsTransaction.insert({});
    }
  });
  res.json({ count: db.UnitTestsTransaction.count() });
});

module.context.use(router);

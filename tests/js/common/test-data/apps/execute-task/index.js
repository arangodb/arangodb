const createRouter = require('@arangodb/foxx/router');
const router = createRouter();

router.get('/execute', function (req, res) {
  require('@arangodb/tasks').register({
    command: function() {
      require("console").log("this task must not run!");
      throw "peng!";
    }
  });
  res.json({ success: true });
});

module.context.use(router);

const createRouter = require('@arangodb/foxx/router');
const router = createRouter();

router.get('/echo', function (req, res) {
  res.json({ echo: require("@arangodb").db._name() });
});

router.get('/echo-nada', function (req, res) {
  res.json({});
});

module.context.use(router);

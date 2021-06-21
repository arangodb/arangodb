const createRouter = require('@arangodb/foxx/router');
const router = createRouter();

router.get('/echo', function (req, res) {
  res.json({ db: require("@arangodb").db._name() });
});

module.context.use(router);

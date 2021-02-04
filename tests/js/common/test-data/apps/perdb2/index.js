
const createRouter = require('@arangodb/foxx/router');
const router = createRouter();

router.get('/echo', function (req, res) {
  res.json({ echo: true });
});

router.get('/echo-piff', function (req, res) {
  res.json({ piff: true });
});

router.get('/echo-nada', function (req, res) {
  res.json({});
});

module.context.use(router);

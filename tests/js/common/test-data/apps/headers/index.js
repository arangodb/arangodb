
const createRouter = require('@arangodb/foxx/router');

const router = createRouter();

router.get('/header-echo', function (req, res) {
  res.json(req.headers);
});
router.post('/header-echo', function (req, res) {
  res.json(req.headers);
});

router.all('/header-empty', function (req, res) {
  // do nothing
});

router.all('/header-automatic', function (req, res) {
  res.set('x-foobar', 'baz');
  res.set('x-nofoobar', 'baz');
});

router.all('/header-manual', function (req, res) {
  res.set('access-control-allow-credentials', 'false');
  res.set('access-control-expose-headers', 'x-foobar');
  res.set('x-foobar', 'baz');
  res.set('x-nofoobar', 'baz');
});

module.context.use(router);

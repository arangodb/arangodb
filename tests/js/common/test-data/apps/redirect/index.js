const createRouter = require('@arangodb/foxx/router');

const router = createRouter();

router.post('/redirect', function (req, res) {
  res.set('location', '/test-redirect/first-redirection');
  res.status(302);
  return res;
});

router.post('/first-redirection', function (req, res) {
  res.set('location', '/test-redirect/second-redirection');
  res.status(302);
  return res;
});

router.post('/second-redirection', function (req, res) {
  res.body = req.body;
  res.status(200);
  return res;
});

module.context.use(router);

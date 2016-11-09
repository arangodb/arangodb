
const createRouter = require('@arangodb/foxx/router');

const router = createRouter();

router.get('/header-echo', function (req, res) {
  Object.keys(req.headers).forEach(function(key) {
    if (key.match(/^[xX]-/)) {
      res.headers[key] = req.headers[key];
    }
  });
});

router.get('/header-static', function (req, res) {
  res.headers['x-foobar'] = 'baz';
});

router.get('/header-cors', function (req, res) {
  res.headers['access-control-expose-headers'] = 'x-session-id';
});

module.context.use(router);

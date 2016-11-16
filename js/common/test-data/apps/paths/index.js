'use strict';
const createRouter = require('@arangodb/foxx/router');
const router = createRouter();
module.context.use(router);

router.get('/hello/:name', function (req, res) {
  res.json({name: req.pathParams.name});
});

router.get('/suffix/*', function (req, res) {
  res.json({suffix: req.suffix});
});

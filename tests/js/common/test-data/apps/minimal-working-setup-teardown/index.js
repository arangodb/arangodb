'use strict';
const router = require('@arangodb/foxx/router')();
module.context.use(router);

router.get('/test', function (req, res) {
  res.json(true);
});

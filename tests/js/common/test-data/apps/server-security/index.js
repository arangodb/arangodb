'use strict';
const router = require('@arangodb/foxx/router')();
const internal = require('internal');
module.context.use(router);

router.get('/test', function (req, res) {
  res.json(true);
});

router.get('/pid', function (req, res) {
  res.json(internal.getPid());
});

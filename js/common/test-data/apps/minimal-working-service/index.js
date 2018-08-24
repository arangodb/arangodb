'use strict';
const router = require('@arangodb/foxx/router')();
module.context.use(router);
router.get((req, res) => {
  res.send({hello: 'world'});
});

router.put((req, res) => {
  let db = require('internal').db;
  res.send({hello: db._query('RETURN CURRENT_USER()')});
});

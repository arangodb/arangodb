'use strict';

const createRouter = require('@arangodb/foxx/router');
const router = createRouter();

router.get(function (req, res) {
  res.json({user: req.arangoUser});
});

module.context.use(router);

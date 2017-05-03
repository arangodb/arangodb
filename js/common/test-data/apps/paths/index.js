'use strict';
const createRouter = require('@arangodb/foxx/router');
const router = createRouter();
module.context.use(router);

router.get(`/${encodeURIComponent('hellö')}/:name`, function (req, res) {
  res.json({crazy: req.pathParams.name});
});

router.get('/hellö/:name', function (req, res) {
  res.json({encoded: req.pathParams.name});
});

router.get('/hello/:name', function (req, res) {
  res.json({raw: req.pathParams.name});
});

router.get('/h+llo/:name', function (req, res) {
  res.json({plus: req.pathParams.name});
});

router.get('/suffix/*', function (req, res) {
  res.json({suffix: req.suffix});
});

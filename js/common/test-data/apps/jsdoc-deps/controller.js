'use strict';
var FoxxApplication = require("org/arangodb/foxx").Controller;
var controller = new FoxxApplication(applicationContext);
var obj = require('./jsdoc-dep');

controller.get('/test', function (req, res) {
  res.json(obj);
});

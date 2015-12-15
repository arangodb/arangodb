var FoxxApplication = require("@arangodb/foxx").Controller;
var controller = new FoxxApplication(applicationContext);

controller.get('/test', function (req, res) {
  'use strict';
  res.json(true);
});

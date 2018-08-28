'use strict';
const path = require('path');
const internal = require('internal');
const utils = require('@arangodb/foxx/manager-utils');
const router = require('@arangodb/foxx/router')();

const basePath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'minimal-working-service');
const servicePath = utils.zipDirectory(basePath);

module.context.use(router);
router.get('zip', (req, res) => {
  res.download(servicePath, 'service.zip');
})
.response(200, ['application/zip']);

router.get('js', (req, res) => {
  res.download(path.resolve(basePath, 'index.js'), 'service.js');
})
.response(200, ['application/javascript']);

'use strict';
const fs = require('fs');
const createRouter = require('@arangodb/foxx/router');

const router = createRouter();

// GET /bundle returns a zip bundle of this service, which can be installed again under another mount
router.get('/bundle', function (req, res) {
  const bundle = fs.zipDirectory(module.context.basePath);
  try {
    res.sendFile(bundle);
  } finally {
    fs.remove(bundle);
  }
});

module.context.use(router);

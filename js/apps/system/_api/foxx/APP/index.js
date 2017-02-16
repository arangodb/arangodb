'use strict';
const _ = require('lodash');
const dd = require('dedent');
const fs = require('fs');
const joi = require('joi');

const actions = require('@arangodb/actions');
const ArangoError = require('@arangodb').ArangoError;
const errors = require('@arangodb').errors;
const jsonml2xml = require('@arangodb/util').jsonml2xml;
const swaggerJson = require('@arangodb/foxx/legacy/swagger').swaggerJson;
const FoxxManager = require('@arangodb/foxx/manager');
const FoxxService = require('@arangodb/foxx/service');
const createRouter = require('@arangodb/foxx/router');
const reporters = Object.keys(require('@arangodb/mocha').reporters);
const schemas = require('./schemas');

const router = createRouter();
module.context.registerType('multipart/form-data', require('./multipart'));
module.context.use(router);

const LDJSON = 'application/x-ldjson';

const legacyErrors = new Map([
  [errors.ERROR_SERVICE_INVALID_NAME.code, errors.ERROR_SERVICE_SOURCE_NOT_FOUND.code],
  [errors.ERROR_SERVICE_INVALID_MOUNT.code, errors.ERROR_INVALID_MOUNTPOINT.code],
  [errors.ERROR_SERVICE_DOWNLOAD_FAILED.code, errors.ERROR_SERVICE_SOURCE_ERROR.code],
  [errors.ERROR_SERVICE_UPLOAD_FAILED.code, errors.ERROR_SERVICE_SOURCE_ERROR.code]
]);

const serviceToJson = (service) => (
  {
    mount: service.mount,
    path: service.basePath,
    name: service.manifest.name,
    version: service.manifest.version,
    development: service.isDevelopment,
    legacy: service.legacy,
    manifest: service.manifest,
    checksum: service.checksum,
    options: _.pick(service.options, ['configuration', 'dependencies'])
  }
);

function writeUploadToTempFile (buffer) {
  const filename = fs.getTempFile('foxx-manager', true);
  fs.writeFileSync(filename, buffer);
  return filename;
}

function prepareServiceRequestBody (req, res, next) {
  if (req.body.source instanceof Buffer) {
    req.body.source = writeUploadToTempFile(req.body.source);
  }
  try {
    if (req.body.dependencies) {
      req.body.dependencies = JSON.parse(req.body.dependencies);
    }
    if (req.body.configuration) {
      req.body.configuration = JSON.parse(req.body.configuration);
    }
  } catch (e) {
    throw new ArangoError({
      errorNum: errors.ERROR_SERVICE_OPTIONS_MALFORMED.code,
      errorMessage: dd`
        ${errors.ERROR_SERVICE_OPTIONS_MALFORMED.message}
        Details: ${e.message}
      `
    }, {cause: e});
  }
  next();
}

router.use((req, res, next) => {
  try {
    next();
  } catch (e) {
    if (e.isArangoError) {
      const errorNum = legacyErrors.get(e.errorNum) || e.errorNum;
      const status = actions.arangoErrorToHttpCode(errorNum);
      res.throw(status, e.errorMessage, {errorNum, cause: e});
    }
    throw e;
  }
});

router.get((req, res) => {
  res.json(
    FoxxManager.installedServices()
    .map((service) => (
      {
        mount: service.mount,
        name: service.manifest.name,
        version: service.manifest.version,
        provides: service.manifest.provides || {},
        development: service.isDevelopment,
        legacy: service.legacy
      }
    ))
  );
})
.response(200, joi.array().items(schemas.shortInfo).required(), `Array of service descriptions.`)
.summary(`List installed services`)
.description(dd`
  Fetches a list of services installed in the current database.
`);

if (FoxxManager.isFoxxmaster()) {
  router.post(prepareServiceRequestBody, (req, res) => {
    const mount = req.queryParams.mount;
    FoxxManager.install(req.body.source, mount, _.omit(req.queryParams, ['mount', 'development']));
    if (req.body.configuration) {
      FoxxManager.setConfiguration(mount, {configuration: req.body.configuration, replace: true});
    }
    if (req.body.dependencies) {
      FoxxManager.setDependencies(mount, {dependencies: req.body.dependencies, replace: true});
    }
    if (req.queryParams.development) {
      FoxxManager.development(mount);
    }
    const service = FoxxManager.lookupService(mount);
    res.json(serviceToJson(service));
  })
  .body(schemas.service, ['multipart/form-data', 'application/json'], `Service to be installed.`)
  .queryParam('mount', schemas.mount, `Mount path the service should be installed at.`)
  .queryParam('development', schemas.flag.default(false), `Enable development mode.`)
  .queryParam('setup', schemas.flag.default(true), `Run the service's setup script.`)
  .queryParam('legacy', schemas.flag.default(false), `Service should be installed in 2.8 legacy compatibility mode.`)
  .response(201, schemas.fullInfo, `Description of the installed service.`)
  .summary(`Install new service`)
  .description(dd`
    Installs the given new service at the given mount path.

    The service source can be specified as either an absolute local file path,
    a fully qualified URL reachable from the database server,
    or as a binary zip file using multipart form upload.
  `);
} else {
  router.post(FoxxManager.proxyToFoxxmaster);
}

const instanceRouter = createRouter();
instanceRouter.use((req, res, next) => {
  const mount = req.queryParams.mount;
  try {
    req.service = FoxxManager.lookupService(mount);
  } catch (e) {
    res.throw(400, `No service installed at mount path "${mount}".`, e);
  }
  next();
})
.queryParam('mount', schemas.mount, `Mount path of the installed service.`);
router.use(instanceRouter);

const serviceRouter = createRouter();
instanceRouter.use('/service', serviceRouter);

serviceRouter.get((req, res) => {
  res.json(serviceToJson(req.service));
})
.response(200, schemas.fullInfo, `Description of the service.`)
.summary(`Service description`)
.description(dd`
  Fetches detailed information for the service at the given mount path.
`);

if (FoxxManager.isFoxxmaster()) {
  serviceRouter.patch(prepareServiceRequestBody, (req, res) => {
    const mount = req.queryParams.mount;
    FoxxManager.upgrade(req.body.source, mount, _.omit(req.queryParams, ['mount']));
    if (req.body.configuration) {
      FoxxManager.setConfiguration(mount, {configuration: req.body.configuration, replace: false});
    }
    if (req.body.dependencies) {
      FoxxManager.setDependencies(mount, {dependencies: req.body.dependencies, replace: false});
    }
    const service = FoxxManager.lookupService(mount);
    res.json(serviceToJson(service));
  })
  .body(schemas.service, ['multipart/form-data', 'application/json'], `Service to be installed.`)
  .queryParam('teardown', schemas.flag.default(false), `Run the old service's teardown script.`)
  .queryParam('setup', schemas.flag.default(true), `Run the new service's setup script.`)
  .queryParam('legacy', schemas.flag.default(false), `Service should be installed in 2.8 legacy compatibility mode.`)
  .response(200, schemas.fullInfo, `Description of the new service.`)
  .summary(`Upgrade service`)
  .description(dd`
    Installs the given new service on top of the service currently installed at the given mount path.
    This is only recommended for switching between different versions of the same service.

    The service source can be specified as either an absolute local file path,
    a fully qualified URL reachable from the database server,
    or as a binary zip file using multipart form upload.
  `);

  serviceRouter.put(prepareServiceRequestBody, (req, res) => {
    const mount = req.queryParams.mount;
    FoxxManager.replace(req.body.source, mount, _.omit(req.queryParams, ['mount']));
    if (req.body.configuration) {
      FoxxManager.setConfiguration(mount, {configuration: req.body.configuration, replace: true});
    }
    if (req.body.dependencies) {
      FoxxManager.setDependencies(mount, {dependencies: req.body.dependencies, replace: true});
    }
    const service = FoxxManager.lookupService(mount);
    res.json(serviceToJson(service));
  })
  .body(schemas.service, ['multipart/form-data', 'application/json'], `Service to be installed.`)
  .queryParam('teardown', schemas.flag.default(true), `Run the old service's teardown script.`)
  .queryParam('setup', schemas.flag.default(true), `Run the new service's setup script.`)
  .queryParam('legacy', schemas.flag.default(false), `Service should be installed in 2.8 legacy compatibility mode.`)
  .response(200, schemas.fullInfo, `Description of the new service.`)
  .summary(`Replace service`)
  .description(dd`
    Removes the service at the given mount path from the database and file system.
    Then installs the given new service at the same mount path.

    The service source can be specified as either an absolute local file path,
    a fully qualified URL reachable from the database server,
    or as a binary zip file using multipart form upload.
  `);

  serviceRouter.delete((req, res) => {
    FoxxManager.uninstall(
      req.queryParams.mount,
      _.omit(req.queryParams, ['mount'])
    );
    res.status(204);
  })
  .queryParam('teardown', schemas.flag.default(true), `Run the service's teardown script.`)
  .response(204, null, `Empty response.`)
  .summary(`Uninstall service`)
  .description(dd`
    Removes the service at the given mount path from the database and file system.
  `);
} else {
  serviceRouter.patch(FoxxManager.proxyToFoxxmaster);
  serviceRouter.put(FoxxManager.proxyToFoxxmaster);
  serviceRouter.delete(FoxxManager.proxyToFoxxmaster);
}

const configRouter = createRouter();
instanceRouter.use('/configuration', configRouter)
.response(200, schemas.configs, `Configuration options of the service.`);

configRouter.get((req, res) => {
  res.json(req.service.getConfiguration());
})
.summary(`Get configuration options`)
.description(dd`
  Fetches the current configuration for the service at the given mount path.
`);

if (FoxxManager.isFoxxmaster()) {
  configRouter.patch((req, res) => {
    const warnings = FoxxManager.setConfiguration(req.service.mount, {
      configuration: req.body,
      replace: false
    });
    const values = req.service.getConfiguration(true);
    res.json({values, warnings});
  })
  .body(joi.object().required(), `Object mapping configuration names to values.`)
  .summary(`Update configuration options`)
  .description(dd`
    Replaces the given service's configuration.
    Any omitted options will be ignored.
  `);

  configRouter.put((req, res) => {
    const warnings = FoxxManager.setConfiguration(req.service.mount, {
      configuration: req.body,
      replace: true
    });
    const values = req.service.getConfiguration(true);
    res.json({values, warnings});
  })
  .body(joi.object().required(), `Object mapping configuration names to values.`)
  .summary(`Replace configuration options`)
  .description(dd`
    Replaces the given service's configuration completely.
    Any omitted options will be reset to their default values or marked as unconfigured.
  `);
} else {
  configRouter.patch(FoxxManager.proxyToFoxxmaster);
  configRouter.put(FoxxManager.proxyToFoxxmaster);
}

const depsRouter = createRouter();
instanceRouter.use('/dependencies', depsRouter)
.response(200, schemas.deps, `Dependency options of the service.`);

depsRouter.get((req, res) => {
  res.json(req.service.getDependencies());
})
.summary(`Get dependency options`)
.description(dd`
  Fetches the current dependencies for service at the given mount path.
`);

if (FoxxManager.isFoxxmaster()) {
  depsRouter.patch((req, res) => {
    const warnings = FoxxManager.setDependencies(req.service.mount, {
      dependencies: req.body,
      replace: true
    });
    const values = req.service.getDependencies(true);
    res.json({values, warnings});
  })
  .body(joi.object().required(), `Object mapping dependency aliases to mount paths.`)
  .summary(`Update dependency options`)
  .description(dd`
    Replaces the given service's dependencies.
    Any omitted dependencies will be ignored.
  `);

  depsRouter.put((req, res) => {
    const warnings = FoxxManager.setDependencies(req.service.mount, {
      dependencies: req.body,
      replace: true
    });
    const values = req.service.getDependencies(true);
    res.json({values, warnings});
  })
  .body(joi.object().required(), `Object mapping dependency aliases to mount paths.`)
  .summary(`Replace dependency options`)
  .description(dd`
    Replaces the given service's dependencies completely.
    Any omitted dependencies will be disabled.
  `);
} else {
  depsRouter.patch(FoxxManager.proxyToFoxxmaster);
  depsRouter.put(FoxxManager.proxyToFoxxmaster);
}

const devRouter = createRouter();
instanceRouter.use('/development', devRouter)
.response(200, schemas.fullInfo, `Description of the service.`);

devRouter.post((req, res) => {
  const service = FoxxManager.development(req.service.mount);
  res.json(serviceToJson(service));
})
.summary(`Enable development mode`)
.description(dd`
  Puts the service into development mode.
  The service will be re-installed from the filesystem for each request.
`);

devRouter.delete((req, res) => {
  const service = FoxxManager.production(req.service.mount);
  res.json(serviceToJson(service));
})
.summary(`Disable development mode`)
.description(dd`
  Puts the service at the given mount path into production mode.
  Changes to the service's code will no longer be reflected automatically.
`);

const scriptsRouter = createRouter();
instanceRouter.use('/scripts', scriptsRouter);

scriptsRouter.get((req, res) => {
  res.json(req.service.getScripts());
})
.response(200, joi.array().items(joi.object({
  name: joi.string().required().description(`Script name`),
  title: joi.string().required().description(`Human-readable script name`)
}).required()).required(), `List of scripts available on the service.`)
.summary(`List service scripts`)
.description(dd`
  Fetches a list of the scripts defined by the service.
`);

scriptsRouter.post('/:name', (req, res) => {
  const service = req.service;
  const scriptName = req.pathParams.name;
  res.json(FoxxManager.runScript(scriptName, service.mount, req.body) || null);
})
.body(joi.any(), `Optional script arguments.`)
.pathParam('name', joi.string().required(), `Name of the script to run`)
.response(200, joi.any().default(null), `Script result if any.`)
.summary(`Run service script`)
.description(dd`
  Runs the given script for the service at the given mount path.
  Returns the exports of the script, if any.
`);

instanceRouter.post('/tests', (req, res) => {
  const service = req.service;
  const reporter = req.queryParams.reporter || null;
  const result = FoxxManager.runTests(service.mount, {reporter});
  if (reporter === 'stream' && req.accepts(LDJSON, 'json') === LDJSON) {
    res.type(LDJSON);
    for (const row of result) {
      res.write(JSON.stringify(row) + '\r\n');
    }
  } else if (reporter === 'xunit' && req.accepts('xml', 'json') === 'xml') {
    res.type('xml');
    res.write('<?xml version="1.0" encoding="utf-8"?>\n');
    res.write(jsonml2xml(result) + '\n');
  } else if (reporter === 'tap' && req.accepts('text', 'json') === 'text') {
    res.type('text');
    for (const row of result) {
      res.write(row + '\n');
    }
  } else {
    res.json(result);
  }
})
.queryParam('reporter', joi.only(...reporters).optional(), `Test reporter to use`)
.response(200, ['json', LDJSON, 'xml', 'text'], `Test results.`)
.summary(`Run service tests`)
.description(dd`
  Runs the tests for the service at the given mount path and returns the results.
`);

instanceRouter.get('/bundle', (req, res) => {
  const service = req.service;
  if (!fs.isFile(service.bundlePath)) {
    if (!service.mount.startsWith('/_')) {
      res.throw(404, 'Bundle not available');
    }
    FoxxManager._createServiceBundle(service.mount);
  }
  const checksum = `"${FoxxService.checksum(service.mount)}"`;
  if (req.get('if-none-match') === checksum) {
    res.status(304);
    return;
  }
  if (req.get('if-match') && req.get('if-match') !== checksum) {
    res.throw(404, 'No matching bundle available');
  }
  res.set('etag', checksum);
  const name = service.mount.replace(/^\/|\/$/g, '').replace(/\//g, '_');
  res.download(service.bundlePath, `${name}.zip`);
})
.response(200, ['application/zip'], `Zip bundle of the service.`)
.header('if-match', joi.string().optional(), `Only return bundle matching the given ETag.`)
.header('if-none-match', joi.string().optional(), `Only return bundle not matching the given ETag.`)
.summary(`Download service bundle`)
.description(dd`
  Downloads a zip bundle of the service directory.
`);

instanceRouter.get('/readme', (req, res) => {
  const service = req.service;
  res.send(service.readme);
})
.response(200, ['text/plain'], `Raw README contents.`)
.summary(`Service README`)
.description(dd`
  Fetches the service's README or README.md file's contents if any.
`);

instanceRouter.get('/swagger', (req, res) => {
  swaggerJson(req, res, {
    mount: req.service.mount
  });
})
.response(200, joi.object(), `Service Swagger description.`)
.summary(`Swagger description`)
.description(dd`
  Fetches the Swagger API description for the service at the given mount path.
`);

const localRouter = createRouter();
router.use('/_local', localRouter);

localRouter.post((req, res) => {
  const result = {};
  for (const mount of Object.keys(req.body)) {
    const coordIds = req.body[mount];
    result[mount] = FoxxManager._installLocal(mount, coordIds);
  }
  FoxxManager._reloadRouting();
  res.json(result);
})
.body(joi.object());

localRouter.post('/service', (req, res) => {
  FoxxManager._reloadRouting();
})
.queryParam('mount', schemas.mount);

localRouter.delete('/service', (req, res) => {
  FoxxManager._uninstallLocal(req.queryParams.mount);
  FoxxManager._reloadRouting();
})
.queryParam('mount', schemas.mount);

localRouter.get('/status', (req, res) => {
  const ready = global.KEY_GET('foxx', 'ready');
  if (ready || FoxxManager.isFoxxmaster()) {
    res.json({ready});
    return;
  }
  FoxxManager.proxyToFoxxmaster(req, res);
  if (res.statusCode < 400) {
    const result = JSON.parse(res.body.toString('utf-8'));
    if (result.ready) {
      global.KEY_SET('foxx', 'ready', result.ready);
    }
  }
});

localRouter.get('/checksums', (req, res) => {
  const mountParam = req.queryParams.mount || [];
  const mounts = Array.isArray(mountParam) ? mountParam : [mountParam];
  const checksums = {};
  for (const mount of mounts) {
    try {
      checksums[mount] = FoxxService.checksum(mount);
    } catch (e) {
    }
  }
  res.json(checksums);
})
.queryParam('mount', joi.alternatives(
  joi.array().items(schemas.mount),
  schemas.mount
));

if (FoxxManager.isFoxxmaster()) {
  localRouter.post('/heal', (req, res) => {
    FoxxManager.heal();
  });
} else {
  localRouter.post('/heal', FoxxManager.proxyToFoxxmaster);
}

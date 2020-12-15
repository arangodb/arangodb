'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2010-2013 triAGENS GmbH, Cologne, Germany
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////


const _ = require('lodash');
const dd = require('dedent');
const fs = require('fs');
const joi = require('joi');

const internal = require('internal');
const actions = require('@arangodb/actions');
const { ArangoError, errors } = require('@arangodb');
const { jsonml2xml } = require('@arangodb/util');
const { swaggerJson } = require('@arangodb/foxx/legacy/swagger');
const FoxxManager = require('@arangodb/foxx/manager');
const createRouter = require('@arangodb/foxx/router');
const { reporters } = require('@arangodb/mocha');
const schemas = require('./schemas');
const { context } = require('@arangodb/locals');

const router = createRouter();

if (internal.isFoxxApiDisabled()) {
  context.service.router.all('*', (_req, res) => {
    res.throw(403, new ArangoError({
      errorNum: errors.ERROR_SERVICE_API_DISABLED.code,
      errorMessage: errors.ERROR_SERVICE_API_DISABLED.message
    }));
  });
} else {
  context.registerType('multipart/form-data', require('./multipart'));
  context.use(router);
}

const LDJSON = 'application/x-ldjson';

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

function prepareServiceRequestBody (req, res, next) {
  if (req.body instanceof Buffer) {
    req.body = {source: req.body};
  }
  try {
    if (typeof req.body.dependencies === 'string') {
      req.body.dependencies = JSON.parse(req.body.dependencies);
    }
    if (typeof req.body.configuration === 'string') {
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
      const status = actions.arangoErrorToHttpCode(e.errorNum);
      console.errorStack(e);
      res.throw(status, (
        e.codeFrame
        ? `${e.codeFrame}\n${e}`
        : String(e)
      ), {
        errorNum: e.errorNum,
        cause: e
      });
    }
    throw e;
  }
});

router.get((req, res) => {
  res.json(
    FoxxManager.installedServices()
    .filter((service) => (
      !req.queryParams.excludeSystem ||
      !service.mount.startsWith('/_')
    ))
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
.queryParam('excludeSystem', schemas.flag.default(false))
.response(200, joi.array().items(schemas.shortInfo).required());

router.post(prepareServiceRequestBody, (req, res) => {
  const mount = req.queryParams.mount;
  FoxxManager.install(req.body.source, mount, Object.assign(
    _.omit(req.queryParams, ['mount']),
    {
      configuration: req.body.configuration,
      dependencies: req.body.dependencies
    }
  ));
  const service = FoxxManager.lookupService(mount);
  res.json(serviceToJson(service));
  res.status(201);
})
.body(schemas.service, ['application/javascript', 'application/zip', 'multipart/form-data', 'application/json'])
.queryParam('mount', schemas.mount)
.queryParam('development', schemas.flag.default(false))
.queryParam('setup', schemas.flag.default(true))
.queryParam('legacy', schemas.flag.default(false))
.response(201, schemas.fullInfo);

const serviceRouter = createRouter();
serviceRouter.use((req, res, next) => {
  try {
    next();
  } catch (e) {
    if (e.errorNum === errors.ERROR_SERVICE_NOT_FOUND.code) {
      const mount = req.queryParams.mount;
      res.throw(400, `No service installed at mount path "${mount}".`, e);
    }
    throw e;
  }
})
.queryParam('mount', schemas.mount);
router.use('/service', serviceRouter);

serviceRouter.get((req, res) => {
  const mount = req.queryParams.mount;
  const service = FoxxManager.lookupService(mount);
  res.json(serviceToJson(service));
})
.response(200, schemas.fullInfo)
.summary('Service description')
.description(dd`
  Fetches detailed information for the service at the given mount path.
`);

serviceRouter.patch(prepareServiceRequestBody, (req, res) => {
  const mount = req.queryParams.mount;
  FoxxManager.upgrade(req.body.source, mount, Object.assign(
    _.omit(req.queryParams, ['mount']),
    {
      configuration: req.body.configuration,
      dependencies: req.body.dependencies
    }
  ));
  const service = FoxxManager.lookupService(mount);
  res.json(serviceToJson(service));
})
.body(schemas.service, ['application/javascript', 'application/zip', 'multipart/form-data', 'application/json'])
.queryParam('teardown', schemas.flag.default(false))
.queryParam('setup', schemas.flag.default(true))
.queryParam('legacy', schemas.flag.default(false))
.queryParam('force', schemas.flag.default(false))
.response(200, schemas.fullInfo);

serviceRouter.put(prepareServiceRequestBody, (req, res) => {
  const mount = req.queryParams.mount;
  FoxxManager.replace(req.body.source, mount, Object.assign(
    _.omit(req.queryParams, ['mount']),
    {
      configuration: req.body.configuration,
      dependencies: req.body.dependencies
    }
  ));
  const service = FoxxManager.lookupService(mount);
  res.json(serviceToJson(service));
})
.body(schemas.service, ['application/javascript', 'application/zip', 'multipart/form-data', 'application/json'])
.queryParam('teardown', schemas.flag.default(true))
.queryParam('setup', schemas.flag.default(true))
.queryParam('legacy', schemas.flag.default(false))
.queryParam('force', schemas.flag.default(false))
.response(200, schemas.fullInfo);

serviceRouter.delete((req, res) => {
  FoxxManager.uninstall(
    req.queryParams.mount,
    _.omit(req.queryParams, ['mount'])
  );
  res.status(204);
})
.queryParam('teardown', schemas.flag.default(true))
.response(204, null);

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
.queryParam('mount', schemas.mount);
router.use(instanceRouter);

const configRouter = createRouter();
instanceRouter.use('/configuration', configRouter)
.response(200, schemas.configs);

configRouter.get((req, res) => {
  res.json(req.service.getConfiguration(req.queryParams.minimal));
})
.queryParam('minimal', joi.boolean().default(false));

configRouter.patch((req, res) => {
  const warnings = FoxxManager.setConfiguration(req.service.mount, {
    configuration: req.body,
    replace: false
  });
  const values = req.service.getConfiguration(req.queryParams.minimal);
  if (req.queryParams.minimal) {
    res.json({values, warnings});
  } else {
    if (warnings) {
      for (const key of Object.keys(warnings)) {
        values[key].warning = warnings[key];
      }
    }
    res.json(values);
  }
})
.body(joi.object().required())
.queryParam('minimal', joi.boolean().default(true));

configRouter.put((req, res) => {
  const warnings = FoxxManager.setConfiguration(req.service.mount, {
    configuration: req.body,
    replace: true
  });
  const values = req.service.getConfiguration(req.queryParams.minimal);
  if (req.queryParams.minimal) {
    res.json({values, warnings});
  } else {
    if (warnings) {
      for (const key of Object.keys(warnings)) {
        values[key].warning = warnings[key];
      }
    }
    res.json(values);
  }
})
.body(joi.object().required())
.queryParam('minimal', joi.boolean().default(true));

const depsRouter = createRouter();
instanceRouter.use('/dependencies', depsRouter)
.response(200, schemas.deps);

depsRouter.get((req, res) => {
  res.json(req.service.getDependencies(req.queryParams.minimal));
})
.queryParam('minimal', joi.boolean().default(false));

depsRouter.patch((req, res) => {
  const warnings = FoxxManager.setDependencies(req.service.mount, {
    dependencies: req.body,
    replace: false
  });
  const values = req.service.getDependencies(req.queryParams.minimal);
  if (req.queryParams.minimal) {
    res.json({values, warnings});
  } else {
    if (warnings) {
      for (const key of Object.keys(warnings)) {
        values[key].warning = warnings[key];
      }
    }
    res.json(values);
  }
})
.body(joi.object().required())
.queryParam('minimal', joi.boolean().default(true));

depsRouter.put((req, res) => {
  const warnings = FoxxManager.setDependencies(req.service.mount, {
    dependencies: req.body,
    replace: true
  });
  const values = req.service.getDependencies(req.queryParams.minimal);
  if (req.queryParams.minimal) {
    res.json({values, warnings});
  } else {
    if (warnings) {
      for (const key of Object.keys(warnings)) {
        values[key].warning = warnings[key];
      }
    }
    res.json(values);
  }
})
.body(joi.object().required())
.queryParam('minimal', joi.boolean().default(true));

const devRouter = createRouter();
instanceRouter.use('/development', devRouter)
.response(200, schemas.fullInfo);

devRouter.post((req, res) => {
  const service = FoxxManager.development(req.service.mount);
  res.json(serviceToJson(service));
});

devRouter.delete((req, res) => {
  const service = FoxxManager.production(req.service.mount);
  res.json(serviceToJson(service));
});

const scriptsRouter = createRouter();
instanceRouter.use('/scripts', scriptsRouter);

scriptsRouter.get((req, res) => {
  res.json(req.service.getScripts());
})
.response(200, joi.array().items(joi.object({
  name: joi.string().required(),
  title: joi.string().required()
}).required()).required());

scriptsRouter.post('/:name', (req, res) => {
  const service = req.service;
  const scriptName = req.pathParams.name;
  const result = FoxxManager.runScript(scriptName, service.mount, req.body !== undefined ? [req.body] : undefined);
  res.json(result !== undefined ? result : null);
})
.body(joi.any().optional())
.pathParam('name', joi.string().required())
.response(200, joi.any().default(null));

instanceRouter.post('/tests', (req, res) => {
  const service = req.service;
  const idiomatic = req.queryParams.idiomatic;
  const filter = req.queryParams.filter || null;
  const reporter = req.queryParams.reporter || null;
  const result = FoxxManager.runTests(service.mount, {filter, reporter});
  if (reporter === 'stream' && idiomatic !== false && (
    idiomatic || req.accepts(LDJSON, 'json') === LDJSON
  )) {
    res.type(LDJSON);
    for (const row of result) {
      res.write(JSON.stringify(row) + '\r\n');
    }
  } else if (reporter === 'xunit' && idiomatic !== false && (
    idiomatic || req.accepts('xml', 'json') === 'xml'
  )) {
    res.type('xml');
    res.write('<?xml version="1.0" encoding="utf-8"?>\n');
    res.write(jsonml2xml(result) + '\n');
  } else if (reporter === 'tap' && idiomatic !== false && (
    idiomatic || req.accepts('text', 'json') === 'text'
  )) {
    res.type('text');
    for (const row of result) {
      res.write(row + '\n');
    }
  } else {
    res.json(result);
  }
})
.queryParam('filter', joi.string().allow("").optional())
.queryParam('reporter', joi.only(...Object.keys(reporters)).optional())
.queryParam('idiomatic', schemas.flag.optional())
.response(200, ['json', LDJSON, 'xml', 'text']);

instanceRouter.post('/download', (req, res) => {
  const service = req.service;
  const filename = service.mount.replace(/^\/|\/$/g, '').replace(/\//g, '_');
  res.attachment(`${filename}.zip`);
  if (!service.isDevelopment && fs.isFile(service.bundlePath)) {
    res.sendFile(service.bundlePath);
  } else {
    const tempFile = fs.getTempFile('bundles', false);
    FoxxManager._createServiceBundle(service.mount, tempFile);
    res.sendFile(tempFile);
    try {
      fs.remove(tempFile);
    } catch (e) {
      console.warnStack(e, `Failed to remove temporary Foxx download bundle: ${tempFile}`);
    }
  }
})
.response(200, ['application/zip']);

instanceRouter.get('/readme', (req, res) => {
  const service = req.service;
  const readme = service.readme;
  if (readme) {
    res.send(service.readme);
  } else {
    res.status(204);
  }
})
.response(200, ['text/plain'])
.response(204, null);

instanceRouter.get('/swagger', (req, res) => {
  swaggerJson(req, res, {
    mount: req.service.mount
  });
})
.response(200, joi.object());

router.post('/commit', (req, res) => {
  FoxxManager.commitLocalState(req.queryParams.replace);
  res.status(204);
})
.response(204, null);

const localRouter = createRouter();
router.use('/_local', localRouter);

localRouter.post('/heal', (req, res) => {
  FoxxManager.heal();
});

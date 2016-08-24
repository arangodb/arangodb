'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2013 triAGENS GmbH, Cologne, Germany
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Alan Plum
////////////////////////////////////////////////////////////////////////////////

const fs = require('fs');
const joi = require('joi');
const dd = require('dedent');
const internal = require('internal');
const crypto = require('@arangodb/crypto');
const marked = require('marked');
const highlightAuto = require('highlight.js').highlightAuto;
const errors = require('@arangodb').errors;
const FoxxManager = require('@arangodb/foxx/manager');
const fmUtils = require('@arangodb/foxx/manager-utils');
const createRouter = require('@arangodb/foxx/router');

const DEFAULT_THUMBNAIL = module.context.fileName('default-thumbnail.png');

const anonymousRouter = createRouter();
const router = createRouter();
anonymousRouter.use(router);

module.exports = anonymousRouter;

router.use((req, res, next) => {
  if (internal.authenticationEnabled()) {
    if (!req.arangoUser) {
      res.throw('unauthorized');
    }
  }
  next();
})
.header('authorization', joi.string().optional(), 'ArangoDB credentials.');


const foxxRouter = createRouter();
router.use(foxxRouter)
.queryParam('mount', joi.string().required(), dd`
  The mount point of the service. Has to be url-encoded.
`);


const installer = createRouter();
foxxRouter.use(installer)
.queryParam('legacy', joi.boolean().default(false), dd`
  Flag to install the service in legacy mode.
`)
.queryParam('upgrade', joi.boolean().default(false), dd`
  Flag to upgrade the service installed at the mountpoint.
  Triggers setup.
`)
.queryParam('replace', joi.boolean().default(false), dd`
  Flag to replace the service installed at the mountpoint.
  Triggers teardown and setup.
`);


installer.use(function (req, res, next) {
  const mount = decodeURIComponent(req.queryParams.mount);
  const upgrade = req.queryParams.upgrade;
  const replace = req.queryParams.replace;
  next();
  let appInfo, options;
  if (typeof req.body === 'object') {
    appInfo = 'EMPTY';
    options = req.body;
  } else {
    appInfo = req.body;
    options = {};
  }
  options.legacy = req.queryParams.legacy;
  let service;
  try {
    if (upgrade) {
      service = FoxxManager.upgrade(appInfo, mount, options);
    } else if (replace) {
      service = FoxxManager.replace(appInfo, mount, options);
    } else {
      service = FoxxManager.install(appInfo, mount, options);
    }
  } catch (e) {
    if (e.isArangoError && [
      errors.ERROR_MODULE_FAILURE.code,
      errors.ERROR_MALFORMED_MANIFEST_FILE.code,
      errors.ERROR_INVALID_SERVICE_MANIFEST.code
    ].indexOf(e.errorNum) !== -1) {
      res.throw('bad request', e);
    }
    if (
      e.isArangoError &&
      e.errorNum === errors.ERROR_SERVICE_NOT_FOUND.code
    ) {
      res.throw('not found', e);
    }
    if (
      e.isArangoError &&
      e.errorNum === errors.ERROR_SERVICE_MOUNTPOINT_CONFLICT.code
    ) {
      res.throw('conflict', e);
    }
    throw e;
  }
  const configuration = FoxxManager.configuration(mount);
  res.json(Object.assign(
    {error: false, configuration},
    service
  ));
});


installer.put('/store', function (req) {
  req.body = `${req.body.name}:${req.body.version}`;
})
.body(joi.object({
  name: joi.string().required(),
  version: joi.string().required()
}).required(), 'A Foxx service from the store.')
.summary('Install a Foxx from the store')
.description(dd`
  Downloads a Foxx from the store and installs it at the given mount.
`);


installer.put('/git', function (req) {
  req.body = `git:${req.body.url}:${req.body.version}`;
})
.body(joi.object({
  url: joi.string().required(),
  version: joi.string().default('master')
}).required(), 'A GitHub reference.')
.summary('Install a Foxx from Github')
.description(dd`
  Install a Foxx with user/repository and version.
`);


installer.put('/generate', () => {})
.body(joi.object().required(), 'A Foxx generator configuration.')
.summary('Generate a new foxx')
.description(dd`
  Generate a new empty foxx on the given mount point.
`);


installer.put('/zip', function (req) {
  req.body = req.body.zipFile;
})
.body(joi.object({
  zipFile: joi.string().required()
}).required(), 'A zip file path.')
.summary('Install a Foxx from temporary zip file')
.description(dd`
  Install a Foxx from the given zip path.
  This path has to be created via _api/upload.
`);


foxxRouter.delete('/', function (req, res) {
  const mount = decodeURIComponent(req.queryParams.mount);
  const runTeardown = req.queryParams.teardown;
  const service = FoxxManager.uninstall(mount, {
    teardown: runTeardown,
    force: true
  });
  res.json(Object.assign(
    {error: false},
    service
  ));
})
.queryParam('teardown', joi.boolean().default(true))
.summary('Uninstall a Foxx')
.description(dd`
  Uninstall the Foxx at the given mount-point.
`);


router.get('/', function (req, res) {
  const foxxes = FoxxManager.listJson();
  foxxes.forEach((foxx) => {
    const readme = FoxxManager.readme(foxx.mount);
    if (readme) {
      foxx.readme = marked(readme, {
        highlight: (code) => highlightAuto(code).value
      });
    }
  });
  res.json(foxxes);
})
.summary('List all Foxxes')
.description(dd`
  Get a List of all running foxxes.
`);


foxxRouter.get('/thumbnail', function (req, res) {
  const mount = decodeURIComponent(req.queryParams.mount);
  const service = FoxxManager.lookupService(mount);
  res.sendFile(service.thumbnail || DEFAULT_THUMBNAIL);
})
.summary('Get the thumbnail of a Foxx')
.description(dd`
  Request the thumbnail of the given Foxx in order to display it on the screen.
`);


foxxRouter.get('/config', function (req, res) {
  const mount = decodeURIComponent(req.queryParams.mount);
  res.json(FoxxManager.configuration(mount));
})
.summary('Get the configuration for a service')
.description(dd`
  Used to request the configuration options for services.
`);


foxxRouter.patch('/config', function (req, res) {
  const mount = decodeURIComponent(req.queryParams.mount);
  const configuration = req.body;
  res.json(FoxxManager.configure(mount, {configuration}));
})
.body(joi.object().optional(), 'Configuration to apply.')
.summary('Set the configuration for a service')
.description(dd`
  Used to overwrite the configuration options for services.
`);


foxxRouter.get('/deps', function (req, res) {
  const mount = decodeURIComponent(req.queryParams.mount);
  res.json(FoxxManager.dependencies(mount));
})
.summary('Get the dependencies for a service')
.description(dd`
  Used to request the dependencies options for services.
`);


foxxRouter.patch('/deps', function (req, res) {
  const mount = decodeURIComponent(req.queryParams.mount);
  const dependencies = req.body;
  res.json(FoxxManager.updateDeps(mount, {dependencies}));
})
.body(joi.object().optional(), 'Dependency settings to apply.')
.summary('Set the dependencies for a service')
.description(dd`
  Used to overwrite the dependencies options for services.
`);


foxxRouter.post('/tests', function (req, res) {
  const mount = decodeURIComponent(req.queryParams.mount);
  res.json(FoxxManager.runTests(mount, req.body));
})
.body(joi.object().optional(), 'Options to pass to the test runner.')
.summary('Run tests for a service')
.description(dd`
  Used to run the tests of a service.
`);


foxxRouter.post('/scripts/:name', function (req, res) {
  const mount = decodeURIComponent(req.queryParams.mount);
  const name = req.pathParams.name;
  try {
    res.json(FoxxManager.runScript(name, mount, req.body));
  } catch (e) {
    throw e.cause || e;
  }
})
.pathParam('name', joi.string().required(), 'The name of a service\'s script to run.')
.body(joi.any().default(null), 'Options to pass to the script.')
.summary('Run a script for a service')
.description(dd`
  Used to trigger any script of a service.
`);


foxxRouter.patch('/devel', function (req, res) {
  const mount = decodeURIComponent(req.queryParams.mount);
  const activate = Boolean(req.body);
  res.json(FoxxManager[activate ? 'development' : 'production'](mount));
})
.body(joi.boolean().optional())
.summary('Activate/Deactivate development mode for a service')
.description(dd`
  Used to toggle between production and development mode.
`);


router.get('/fishbowl', function (req, res) {
  try {
    FoxxManager.update();
  } catch (e) {
    console.warnLines(`Failed to update Foxx store: ${e.stack}`);
  }
  res.json(FoxxManager.availableJson());
})
.summary('List of all foxx services submitted to the Foxx store.')
.description(dd`
  This function contacts the fishbowl and reports which services are available for install.
`);


router.post('/download/nonce', function (req, res) {
  const nonce = crypto.createNonce();
  res.status('created');
  res.json({nonce});
})
.response('created', joi.object({nonce: joi.string().required()}).required(), 'The created nonce.')
.summary('Creates a nonce for downloading the service')
.description(dd`
  Creates a cryptographic nonce that can be used to download the service without authentication.
`);


anonymousRouter.get('/download/zip', function (req, res) {
  const nonce = decodeURIComponent(req.queryParams.nonce);
  const checked = nonce && crypto.checkAndMarkNonce(nonce);
  if (!checked) {
    res.throw(403, 'Nonce missing or invalid');
  }
  const mount = decodeURIComponent(req.queryParams.mount);
  const service = FoxxManager.lookupService(mount);
  const dir = fs.join(fs.makeAbsolute(service.root), service.path);
  const zipPath = fmUtils.zipDirectory(dir);
  const name = mount.replace(/^\/|\/$/g, '').replace(/\//g, '_');
  res.download(zipPath, `${name}_${service.manifest.version}.zip`);
})
.queryParam('nonce', joi.string().required(), 'Cryptographic nonce that authorizes the download.')
.summary('Download a service as zip archive')
.description(dd`
  Download a foxx service packed in a zip archive.
`);

anonymousRouter.use('/docs/standalone', module.context.createDocumentationRouter((req, res) => {
  if (req.suffix === 'swagger.json' && !req.arangoUser && internal.authenticationEnabled()) {
    res.throw('unauthorized');
  }
  return {
    mount: decodeURIComponent(req.queryParams.mount)
  };
}));

anonymousRouter.use('/docs', module.context.createDocumentationRouter((req, res) => {
  if (req.suffix === 'swagger.json' && !req.arangoUser && internal.authenticationEnabled()) {
    res.throw('unauthorized');
  }
  return {
    mount: decodeURIComponent(req.queryParams.mount),
    indexFile: 'index-alt.html'
  };
}));

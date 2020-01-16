'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2010-2013 triAGENS GmbH, Cologne, Germany
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / @author Michael Hackstein
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const fs = require('fs');
const joi = require('joi');
const dd = require('dedent');
const crypto = require('@arangodb/crypto');
const errors = require('@arangodb').errors;
const FoxxManager = require('@arangodb/foxx/manager');
const store = require('@arangodb/foxx/store');
const FoxxGenerator = require('./generator');
const fmu = require('@arangodb/foxx/manager-utils');
const createRouter = require('@arangodb/foxx/router');
const joinPath = require('path').join;

const DEFAULT_THUMBNAIL = module.context.fileName('default-thumbnail.png');

const anonymousRouter = createRouter();
const router = createRouter();
anonymousRouter.use(router);

module.exports = anonymousRouter;

router.use((req, res, next) => {
  if (internal.authenticationEnabled()) {
    if (!req.authorized) {
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
  Flag to upgrade the service installed at the mount point.
`)
.queryParam('replace', joi.boolean().default(false), dd`
  Flag to replace the service installed at the mount point.
`)
.queryParam('setup', joi.boolean().default(true), dd`
  Flag to run setup after install.
`)
.queryParam('teardown', joi.boolean().default(false), dd`
  Flag to run teardown before replace/upgrade.
`);

installer.use(function (req, res, next) {
  const mount = decodeURIComponent(req.queryParams.mount);
  const upgrade = req.queryParams.upgrade;
  const replace = req.queryParams.replace;
  next();
  const options = {};
  const appInfo = req.body;
  options.legacy = req.queryParams.legacy;
  options.setup = req.queryParams.setup;
  options.teardown = req.queryParams.teardown;
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
  const configuration = service.getConfiguration();
  res.json(Object.assign({
    error: false,
    configuration
  }, service.simpleJSON()));
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
  const baseUrl = process.env.FOXX_BASE_URL || 'https://github.com/';
  req.body = `${baseUrl}${req.body.url}/archive/${req.body.version || 'master'}.zip`;
})
.body(joi.object({
  url: joi.string().required(),
  version: joi.string().default('master')
}).required(), 'A GitHub reference.')
.summary('Install a Foxx from Github')
.description(dd`
  Install a Foxx with user/repository and version.
`);

installer.put('/url', function (req) {
  req.body = `${req.body.url}`;
})
.body(joi.object({
  url: joi.string().required(),
  version: joi.string().default('master')
}).required(), '')
.summary('Install a Foxx from URL')
.description(dd`
  Install a Foxx from URL.
`);

installer.put('/generate', (req, res) => {
  const tempDir = fs.getTempFile('aardvark', false);
  const generated = FoxxGenerator.generate(req.body);
  FoxxGenerator.write(tempDir, generated.files, generated.folders);
  const tempFile = fmu.zipDirectory(tempDir);
  req.body = fs.readFileSync(tempFile);
  try {
    fs.removeDirectoryRecursive(tempDir, true);
  } catch (e) {
    console.warn(`Failed to remove temporary Foxx generator folder: ${tempDir}`);
  }
  try {
    fs.remove(tempFile);
  } catch (e) {
    console.warn(`Failed to remove temporary Foxx generator bundle: ${tempFile}`);
  }
})
.body(joi.object().required(), 'A Foxx generator configuration.')
.summary('Generate a new foxx')
.description(dd`
  Generate a new empty foxx on the given mount point.
`);

installer.put('/zip', function (req) {
  const tempFile = joinPath(fs.getTempPath(), req.body.zipFile);
  req.body = fs.readFileSync(tempFile);
  try {
    fs.remove(tempFile);
  } catch (e) {
    console.warn(`Failed to remove uploaded file: ${tempFile}`);
  }
})
.body(joi.object({
  zipFile: joi.string().required()
}).required(), 'A zip file path.')
.summary('Install a Foxx from temporary zip file')
.description(dd`
  Install a Foxx from the given zip path.
  This path has to be created via _api/upload.
`);

installer.put('/raw', function (req) {
  req.body = req.rawBody;
})
.summary('Install a Foxx from a direct upload')
.description(dd`
  Install a Foxx from raw request body.
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
    service.simpleJSON()
  ));
})
.queryParam('teardown', joi.boolean().default(true))
.summary('Uninstall a Foxx')
.description(dd`
  Uninstall the Foxx at the given mount-point.
`);

router.get('/', function (req, res) {
  res.json(FoxxManager.installedServices().map(service => ({
    mount: service.mount,
    name: service.manifest.name,
    description: service.manifest.description,
    author: service.manifest.author,
    system: service.isSystem,
    development: service.isDevelopment,
    contributors: service.manifest.contributors || false,
    license: service.manifest.license,
    version: service.manifest.version,
    path: service.basePath,
    config: service.getConfiguration(),
    deps: service.getDependencies(),
    scripts: service.getScripts(),
    readme: service.readme
  })));
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
  const service = FoxxManager.lookupService(mount);
  res.json(service.getConfiguration());
})
.summary('Get the configuration for a service')
.description(dd`
  Used to request the configuration options for services.
`);

foxxRouter.get('/deps', function (req, res) {
  const mount = decodeURIComponent(req.queryParams.mount);
  const service = FoxxManager.lookupService(mount);
  const deps = service.getDependencies();
  for (const key of Object.keys(deps)) {
    const dep = deps[key];
    deps[key] = {
      definition: dep,
      title: dep.title,
      current: dep.current
    };
    delete dep.title;
    delete dep.current;
  }
  res.json(deps);
})
.summary('Get the dependencies for a service')
.description(dd`
  Used to request the dependencies options for services.
`);

foxxRouter.patch('/config', function (req, res) {
  const mount = decodeURIComponent(req.queryParams.mount);
  const configuration = req.body;
  const service = FoxxManager.lookupService(mount);
  FoxxManager.setConfiguration(mount, {
    configuration,
    replace: !service.isDevelopment
  });
  res.json(service.getConfiguration());
})
.body(joi.object().optional(), 'Configuration to apply.')
.summary('Set the configuration for a service')
.description(dd`
  Used to overwrite the configuration options for services.
`);

foxxRouter.patch('/deps', function (req, res) {
  const mount = decodeURIComponent(req.queryParams.mount);
  const dependencies = req.body;
  const service = FoxxManager.lookupService(mount);
  FoxxManager.setDependencies(mount, {
    dependencies,
    replace: !service.isDevelopment
  });
  res.json(service.getDependencies());
})
.body(joi.object().optional(), 'Dependency options to apply.')
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
  res.json(FoxxManager[activate ? 'development' : 'production'](mount).simpleJSON());
})
.body(joi.boolean().optional())
.summary('Activate/Deactivate development mode for a service')
.description(dd`
  Used to toggle between production and development mode.
`);

router.get('/fishbowl', function (req, res) {
  if (internal.isFoxxStoreDisabled()) {
    res.json([]);
  } else {
    try {
      store.update();
    } catch (e) {
      console.warn('Failed to update Foxx store from GitHub.');
    }
    res.json(store.availableJson());
  } 
})
.summary('List of all Foxx services submitted to the Foxx store.')
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
  const zipPath = fmu.zipDirectory(dir);
  const name = mount.replace(/^\/|\/$/g, '').replace(/\//g, '_');
  res.download(zipPath, `${name}_${service.manifest.version}.zip`);
})
.queryParam('nonce', joi.string().required(), 'Cryptographic nonce that authorizes the download.')
.summary('Download a service as zip archive')
.description(dd`
  Download a Foxx service packed in a zip archive.
`);

anonymousRouter.use('/docs/standalone', module.context.createDocumentationRouter((req, res) => {
  if (req.suffix === 'swagger.json' && !req.authorized && internal.authenticationEnabled()) {
    res.throw('unauthorized');
  }
  return {
    mount: decodeURIComponent(req.queryParams.mount)
  };
}));

anonymousRouter.use('/docs', module.context.createDocumentationRouter((req, res) => {
  if (req.suffix === 'swagger.json' && !req.authorized && internal.authenticationEnabled()) {
    res.throw('unauthorized');
  }
  return {
    mount: decodeURIComponent(req.queryParams.mount),
    indexFile: 'index.html'
  };
}));

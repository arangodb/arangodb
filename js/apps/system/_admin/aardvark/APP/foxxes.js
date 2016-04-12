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
const marked = require('marked');
const httperr = require('http-errors');
const internal = require('internal');
const highlightAuto = require('highlight.js').highlightAuto;
const FoxxManager = require('@arangodb/foxx/manager');
const fmUtils = require('@arangodb/foxx/manager-utils');
const createRouter = require('@arangodb/foxx/router');

const DEFAULT_THUMBNAIL = module.context.fileName('default-thumbnail.png');

const mountSchema = joi.string().required().description(
  'The mount point of the service. Has to be url-encoded.'
);

const router = createRouter();
module.exports = router;

router.use((req, res, next) => {
  if (!internal.options()['server.disable-authentication'] && !req.session.uid) {
    throw new httperr.Unauthorized();
  }
  next();
});

function installApp(req, res, appInfo, options) {
  var mount = decodeURIComponent(req.params('mount'));
  var upgrade = req.params('upgrade');
  var replace = req.params('replace');
  var service;
  if (upgrade) {
    service = FoxxManager.upgrade(appInfo, mount, options);
  } else if (replace) {
    service = FoxxManager.replace(appInfo, mount, options);
  } else {
    service = FoxxManager.install(appInfo, mount, options);
  }
  var config = FoxxManager.configuration(mount);
  res.json({
    error: false,
    configuration: config,
    name: service.name,
    version: service.version
  });
}

const installer = createRouter();
router.use(installer)
.queryParam('mount', mountSchema)
.queryParam('upgrade', joi.boolean().default(false).description(
  'Trigger to upgrade the service installed at the mountpoint. Triggers setup.'
))
.queryParam('replace', joi.boolean().default(false).description(
  'Trigger to replace the service installed at the mountpoint. Triggers teardown and setup.'
));

installer.put('/store', function (req, res) {
  const content = JSON.parse(req.requestBody);
  const name = content.name;
  const version = content.version;
  installApp(req, res, `${name}:${version}`);
})
.summary('Install a Foxx from the store')
.description('Downloads a Foxx from the store and installs it at the given mount.');

installer.put('/git', function (req, res) {
  const content = JSON.parse(req.requestBody);
  const url = content.url;
  const version = content.version || 'master';
  installApp(req, res, `git:${url}:${version}`);
})
.summary('Install a Foxx from Github')
.description('Install a Foxx with user/repository and version.');

installer.put('/generate', function (req, res) {
  const info = JSON.parse(req.requestBody);
  installApp(req, res, 'EMPTY', info);
})
.summary('Generate a new foxx')
.description('Generate a new empty foxx on the given mount point');

installer.put('/zip', function (req, res) {
  const content = JSON.parse(req.requestBody);
  const file = content.zipFile;
  installApp(req, res, file);
})
.summary('Install a Foxx from temporary zip file')
.description('Install a Foxx from the given zip path. This path has to be created via _api/upload');

router.delete('/', function (req, res) {
  var mount = decodeURIComponent(req.params('mount'));
  var runTeardown = req.parameters.teardown;
  var service = FoxxManager.uninstall(mount, {
    teardown: runTeardown,
    force: true
  });
  res.json({
    error: false,
    name: service.name,
    version: service.version
  });
})
.queryParam('mount', mountSchema)
.queryParam('teardown', joi.boolean().default(true))
.summary('Uninstall a Foxx')
.description('Uninstall the Foxx at the given mount-point.');

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
.description('Get a List of all running foxxes.');

router.get('/thumbnail', function (req, res) {
  var mount = decodeURIComponent(req.params('mount'));
  var service = FoxxManager.lookupApp(mount);
  res.sendFile(service.thumbnail || DEFAULT_THUMBNAIL);
})
.queryParam('mount', mountSchema)
.summary('Get the thumbnail of a Foxx')
.description('Request the thumbnail of the given Foxx in order to display it on the screen.');

router.get('/config', function (req, res) {
  var mount = decodeURIComponent(req.params('mount'));
  res.json(FoxxManager.configuration(mount));
})
.queryParam('mount', mountSchema)
.summary('Get the configuration for a service')
.description('Used to request the configuration options for services');

router.patch('/config', function (req, res) {
  var mount = decodeURIComponent(req.params('mount'));
  var data = req.body;
  res.json(FoxxManager.configure(mount, {configuration: data}));
})
.queryParam('mount', mountSchema)
.summary('Set the configuration for a service')
.description('Used to overwrite the configuration options for services');

router.get('/deps', function (req, res) {
  var mount = decodeURIComponent(req.params('mount'));
  res.json(FoxxManager.dependencies(mount));
})
.queryParam('mount', mountSchema)
.summary('Get the dependencies for a service')
.description('Used to request the dependencies options for services');

router.patch('/deps', function (req, res) {
  var mount = decodeURIComponent(req.params('mount'));
  var data = req.body;
  res.json(FoxxManager.updateDeps(mount, {dependencies: data}));
})
.queryParam('mount', mountSchema)
.summary('Set the dependencies for a service')
.description('Used to overwrite the dependencies options for services');

router.post('/tests', function (req, res) {
  var options = req.body;
  var mount = decodeURIComponent(req.params('mount'));
  res.json(FoxxManager.runTests(mount, options));
})
.queryParam('mount', mountSchema)
.summary('Run tests for a service')
.description('Used to run the tests of a service');

router.post('/scripts/:name', function (req, res) {
  const mount = decodeURIComponent(req.params('mount'));
  const name = req.params('name');
  const argv = req.body;
  try {
    res.json(FoxxManager.runScript(name, mount, argv));
  } catch (e) {
    throw e.cause || e;
  }
})
.queryParam('mount', mountSchema)
.pathParam('name', joi.string().required(), 'The name of a service\'s script to run.')
.body('argv', joi.any().default(null), 'Options to pass to the script.')
.summary('Run a script for a service')
.description('Used to trigger any script of a service');

router.patch('/setup', function (req, res) {
  const mount = decodeURIComponent(req.params('mount'));
  res.json(FoxxManager.runScript('setup', mount));
})
.queryParam('mount', mountSchema)
.summary('Trigger setup script for a service')
.description('Used to trigger the setup script of a service');

router.patch('/teardown', function (req, res) {
  const mount = decodeURIComponent(req.params('mount'));
  res.json(FoxxManager.runScript('teardown', mount));
})
.queryParam('mount', mountSchema)
.summary('Trigger teardown script for a service')
.description('Used to trigger the teardown script of a service');

router.patch('/devel', function (req, res) {
  const mount = decodeURIComponent(req.params('mount'));
  const activate = Boolean(req.body);
  res.json(FoxxManager[activate ? 'development' : 'production'](mount));
})
.queryParam('mount', mountSchema)
.summary('Activate/Deactivate development mode for a service')
.description('Used to toggle between production and development mode');

router.get('/download/zip', function (req, res) {
  var mount = decodeURIComponent(req.params('mount'));
  var service = FoxxManager.lookupApp(mount);
  var dir = fs.join(fs.makeAbsolute(service.root), service.path);
  var zipPath = fmUtils.zipDirectory(dir);
  res.download(zipPath, `${service.name}@${service.version}.zip`);
})
.queryParam('mount', mountSchema)
.summary('Download a service as zip archive')
.description('Download a foxx service packed in a zip archive');

router.get('/fishbowl', function (req, res) {
  FoxxManager.update();
  res.json(FoxxManager.availableJson());
})
.summary('List of all foxx services submitted to the Foxx store.')
.description('This function contacts the fishbowl and reports which services are available for install');

router.get('/docs/standalone/*', module.context.createSwaggerHandler(
  (req) => ({
    appPath: req.queryParams.mount
  })
));

router.get('/docs/*', module.context.createSwaggerHandler(
  (req) => ({
    indexFile: 'index-alt.html',
    appPath: req.queryParams.mount
  })
));

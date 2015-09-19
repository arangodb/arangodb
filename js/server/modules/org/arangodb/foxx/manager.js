/*jshint esnext: true */
/*global ArangoServerState, ArangoClusterInfo, ArangoClusterComm */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx application manager
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Michael Hackstein
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                                           imports
// -----------------------------------------------------------------------------

const R = require('ramda');
const fs = require('fs');
const joi = require('joi');
const util = require('util');
const semver = require('semver');
const utils = require('org/arangodb/foxx/manager-utils');
const store = require('org/arangodb/foxx/store');
const deprecated = require('org/arangodb/deprecated');
const ArangoApp = require('org/arangodb/foxx/arangoApp').ArangoApp;
const TemplateEngine = require('org/arangodb/foxx/templateEngine').Engine;
const routeApp = require('org/arangodb/foxx/routing').routeApp;
const exportApp = require('org/arangodb/foxx/routing').exportApp;
const invalidateExportCache = require('org/arangodb/foxx/routing').invalidateExportCache;
const formatUrl = require('url').format;
const parseUrl = require('url').parse;
const arangodb = require('org/arangodb');
const ArangoError = arangodb.ArangoError;
const db = arangodb.db;
const checkParameter = arangodb.checkParameter;
const errors = arangodb.errors;
const cluster = require('org/arangodb/cluster');
const download = require('internal').download;
const executeGlobalContextFunction = require('internal').executeGlobalContextFunction;
const actions = require('org/arangodb/actions');
const plainServerVersion = require("org/arangodb").plainServerVersion;
const _ = require('underscore');
const Module = require('module');

const throwDownloadError = arangodb.throwDownloadError;
const throwFileNotFound = arangodb.throwFileNotFound;

// Regular expressions for joi patterns
const RE_FQPATH = /^\//;
const RE_EMPTY = /^$/;
const RE_NOT_FQPATH = /^[^\/]/;
const RE_NOT_EMPTY = /./;

const manifestSchema = {
  assets: (
    joi.object().optional()
    .pattern(RE_EMPTY, joi.forbidden())
    .pattern(RE_NOT_EMPTY, (
      joi.object().required()
      .keys({
        files: (
          joi.array().required()
          .items(joi.string().required())
        ),
        contentType: joi.string().optional()
      })
    ))
  ),
  author: joi.string().allow('').default(''),
  configuration: (
    joi.object().optional()
    .pattern(RE_EMPTY, joi.forbidden())
    .pattern(RE_NOT_EMPTY, (
      joi.object().required()
      .keys({
        default: joi.any().optional(),
        type: (
          joi.only(Object.keys(utils.parameterTypes))
          .default('string')
        ),
        description: joi.string().optional(),
        required: joi.boolean().default(true)
      })
    ))
  ),
  contributors: joi.array().optional(),
  controllers: joi.alternatives().try(
    joi.string().optional(),
    (
      joi.object().optional()
      .pattern(RE_NOT_FQPATH, joi.forbidden())
      .pattern(RE_FQPATH, joi.string().required())
    )
  ),
  defaultDocument: joi.string().allow('').allow(null).default(null),
  dependencies: (
    joi.object().optional()
    .pattern(RE_EMPTY, joi.forbidden())
    .pattern(RE_NOT_EMPTY, joi.alternatives().try(
      joi.string().required(),
      joi.object().required()
      .keys({
        name: joi.string().default('*'),
        version: joi.string().default('*'),
        required: joi.boolean().default(true)
      })
    ))
  ),
  description: joi.string().allow('').default(''),
  engines: (
    joi.object().optional()
    .pattern(RE_EMPTY, joi.forbidden())
    .pattern(RE_NOT_EMPTY, joi.string().required())
  ),
  exports: joi.alternatives().try(
    joi.string().optional(),
    (
      joi.object().optional()
      .pattern(RE_EMPTY, joi.forbidden())
      .pattern(RE_NOT_EMPTY, joi.string().required())
    )
  ),
  files: (
    joi.object().optional()
    .pattern(RE_EMPTY, joi.forbidden())
    .pattern(RE_NOT_EMPTY, joi.alternatives().try(joi.string().required(), joi.object().required()))
  ),
  isSystem: joi.boolean().default(false),
  keywords: joi.array().optional(),
  lib: joi.string().default('.'),
  license: joi.string().optional(),
  name: joi.string().regex(/^[-_a-z][-_a-z0-9]*$/i).required(),
  repository: (
    joi.object().optional()
    .keys({
      type: joi.string().required(),
      url: joi.string().required()
    })
  ),
  scripts: (
    joi.object().optional()
    .pattern(RE_EMPTY, joi.forbidden())
    .pattern(RE_NOT_EMPTY, joi.string().required())
    .default(Object, 'empty scripts object')
  ),
  setup: joi.string().optional(), // TODO remove in 2.8
  teardown: joi.string().optional(), // TODO remove in 2.8
  tests: (
    joi.alternatives()
    .try(
      joi.string().required(),
      (
        joi.array().optional()
        .items(joi.string().required())
        .default(Array, 'empty test files array')
      )
    )
  ),
  thumbnail: joi.string().optional(),
  version: joi.string().required(),
  rootElement: joi.boolean().default(false)
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

var appCache = {};
var usedSystemMountPoints = [
  '/_admin/aardvark', // Admin interface.
  '/_system/cerberus', // Password recovery.
  '/_api/gharial', // General_Graph API.
  '/_system/sessions', // Sessions.
  '/_system/users', // Users.
  '/_system/simple-auth' // Authentication.
];

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Searches through a tree of files and returns true for all app roots
////////////////////////////////////////////////////////////////////////////////

function filterAppRoots(folder) {
  return /[\\\/]APP$/i.test(folder) && !/(APP[\\\/])(.*)APP$/i.test(folder);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Trigger reload routing
/// Triggers reloading of routes in this as well as all other threads.
////////////////////////////////////////////////////////////////////////////////

function reloadRouting() {
  executeGlobalContextFunction('reloadRouting');
  actions.reloadRouting();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Resets the app cache
////////////////////////////////////////////////////////////////////////////////

function resetCache() {
  appCache = {};
  invalidateExportCache();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup app in cache
/// Returns either the app or undefined if it is not cached.
////////////////////////////////////////////////////////////////////////////////

function lookupApp(mount) {
  var dbname = arangodb.db._name();
  if (!appCache.hasOwnProperty(dbname) || Object.keys(appCache[dbname]).length === 0) {
    refillCaches(dbname);
  }
  if (!appCache[dbname].hasOwnProperty(mount)) {
    refillCaches(dbname);
    if (appCache[dbname].hasOwnProperty(mount)) {
      return appCache[dbname][mount];
    }
    throw new ArangoError({
      errorNum: errors.ERROR_APP_NOT_FOUND.code,
      errorMessage: 'App not found at: ' + mount
    });
  }
  return appCache[dbname][mount];
}


////////////////////////////////////////////////////////////////////////////////
/// @brief refills the routing cache
////////////////////////////////////////////////////////////////////////////////

function refillCaches(dbname) {
  var cache = appCache[dbname] = {};

  var cursor = utils.getStorage().all();
  var routes = [];

  while (cursor.hasNext()) {
    var config = _.clone(cursor.next());
    var app = new ArangoApp(config);
    var mount = app._mount;
    cache[mount] = app;
    routes.push(mount);
  }

  return routes;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief routes of an foxx
////////////////////////////////////////////////////////////////////////////////

function routes(mount) {
  var app = lookupApp(mount);
  return routeApp(app);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Makes sure all system apps are mounted.
////////////////////////////////////////////////////////////////////////////////

function checkMountedSystemApps(dbname) {
  var i, mount;
  var collection = utils.getStorage();
  for (i = 0; i < usedSystemMountPoints.length; ++i) {
    mount = usedSystemMountPoints[i];
    delete appCache[dbname][mount];
    var definition = collection.firstExample({mount: mount});
    if (definition !== null) {
      collection.remove(definition._key);
    }
    _scanFoxx(mount, {});
    executeAppScript('setup', lookupApp(mount));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check a manifest for completeness
///
/// this implements issue #590: Manifest Lint
////////////////////////////////////////////////////////////////////////////////

function checkManifest(filename, manifest) {
  const serverVersion = plainServerVersion();
  const validationErrors = [];

  Object.keys(manifestSchema).forEach(function (key) {
    let schema = manifestSchema[key];
    let value = manifest[key];
    let result = joi.validate(value, schema);
    if (result.value !== undefined) {
      manifest[key] = result.value;
    }
    if (result.error) {
      let error = result.error.message.replace(/^"value"/, `"${key}"`);
      let message = `Manifest "${filename}": attribute ${error} (was "${util.format(value)}").`;
      validationErrors.push(message);
      console.error(message);
    }
  });

  if (
    manifest.engines
    && manifest.engines.arangodb
    && !semver.satisfies(serverVersion, manifest.engines.arangodb)
   ) {
    console.warn(
      `Manifest "${filename}" for app "${manifest.name}":`
      + ` ArangoDB version ${serverVersion} probably not compatible`
      + ` with expected version ${manifest.engines.arangodb}.`
    );
  }

  if (manifest.setup && manifest.setup !== manifest.scripts.setup) {
    deprecated('2.8', (
      `Manifest "${filename}" for app "${manifest.name}" contains deprecated attribute "setup",`
      + ` use "scripts.setup" instead.`
    ));
    manifest.scripts.setup = manifest.scripts.setup || manifest.setup;
    delete manifest.setup;
  }

  if (manifest.teardown && manifest.teardown !== manifest.scripts.teardown) {
    deprecated('2.8', (
      `Manifest "${filename}" for app "${manifest.name}" contains deprecated attribute "teardown",`
      + ` use "scripts.teardown" instead.`
    ));
    manifest.scripts.teardown = manifest.scripts.teardown || manifest.teardown;
    delete manifest.teardown;
  }

  if (manifest.assets) {
    deprecated('2.8', (
      `Manifest "${filename}" for app "${manifest.name}" contains deprecated attribute "assets",`
      + ` use "files" and an external build tool instead.`
    ));
  }

  Object.keys(manifest).forEach(function (key) {
    if (!manifestSchema[key]) {
      console.warn(`Manifest "${filename}" for app "${manifest.name}": unknown attribute "${key}"`);
    }
  });

  if (validationErrors.length) {
    throw new ArangoError({
      errorNum: errors.ERROR_INVALID_APPLICATION_MANIFEST.code,
      errorMessage: validationErrors.join('\n')
    });
  } else {
    if (manifest.dependencies) {
      Object.keys(manifest.dependencies).forEach(function (key) {
        const dependency = manifest.dependencies[key];
        if (typeof dependency === 'string') {
          const tokens = dependency.split(':');
          manifest.dependencies[key] = {
            name: tokens[0] || '*',
            version: tokens[1] || '*',
            required: true
          };
        }
      });
    }

    if (typeof manifest.controllers === 'string') {
      manifest.controllers = {'/': manifest.controllers};
    }

    if (typeof manifest.tests === 'string') {
      manifest.tests = [manifest.tests];
    }
  }
}



////////////////////////////////////////////////////////////////////////////////
/// @brief validates a manifest file and returns it.
/// All errors are handled including file not found. Returns undefined if manifest is invalid
////////////////////////////////////////////////////////////////////////////////

function validateManifestFile(file) {
  var mf, msg;
  if (!fs.exists(file)) {
    msg = `Cannot find manifest file "${file}"`;
    console.errorLines(msg);
    throwFileNotFound(msg);
  }
  try {
    mf = JSON.parse(fs.read(file));
  } catch (err) {
    let details = String(err.stack || err);
    msg = `Cannot parse app manifest "${file}": ${details}`;
    console.errorLines(msg);
    throw new ArangoError({
      errorNum: errors.ERROR_MALFORMED_MANIFEST_FILE.code,
      errorMessage: msg
    });
  }
  try {
    checkManifest(file, mf);
  } catch (err) {
    console.errorLines(`Manifest file "${file}" is invalid:\n${err.errorMessage}`);
    if (err.stack) {
      console.errorLines(err.stack);
    }
    throw err;
  }
  return mf;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if the mountpoint is reserved for system apps
////////////////////////////////////////////////////////////////////////////////

function isSystemMount(mount) {
  return (/^\/_/).test(mount);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the root path for application. Knows about system apps
////////////////////////////////////////////////////////////////////////////////

function computeRootAppPath(mount) {
  if (isSystemMount(mount)) {
    return Module._systemAppPath;
  }
  return Module._appPath;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief transforms a mount point to a sub-path relative to root
////////////////////////////////////////////////////////////////////////////////

function transformMountToPath(mount) {
  var list = mount.split('/');
  list.push('APP');
  return fs.join.apply(fs, list);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief transforms a sub-path to a mount point
////////////////////////////////////////////////////////////////////////////////

function transformPathToMount(path) {
  var list = path.split(fs.pathSeparator);
  list.pop();
  return '/' + list.join('/');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the application path for mount point
////////////////////////////////////////////////////////////////////////////////

function computeAppPath(mount) {
  var root = computeRootAppPath(mount);
  var mountPath = transformMountToPath(mount);
  return fs.join(root, mountPath);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an app script
////////////////////////////////////////////////////////////////////////////////

function executeAppScript(scriptName, app, argv) {
  var readableName = utils.getReadableName(scriptName);
  var scripts = app._manifest.scripts;

  // Only run setup/teardown scripts if they exist
  if (scripts[scriptName] || (scriptName !== 'setup' && scriptName !== 'teardown')) {
    try {
      return app.loadAppScript(scripts[scriptName], {
        appContext: {
          argv: argv ? (Array.isArray(argv) ? argv : [argv]) : []
        }
      });
    } catch (e) {
      if (!(e.cause || e).statusCode) {
        let details = String((e.cause || e).stack || e.cause || e);
        console.errorLines(`Running script "${readableName}" not possible for mount "${app._mount}":\n${details}`);
      }
      throw e;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a valid app config for validation purposes
////////////////////////////////////////////////////////////////////////////////

function fakeAppConfig(path) {
  var file = fs.join(path, 'manifest.json');
  return {
    id: '__internal',
    root: '',
    path: path,
    options: {},
    mount: '/internal',
    manifest: validateManifestFile(file),
    isSystem: false,
    isDevelopment: false
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the app path and manifest
////////////////////////////////////////////////////////////////////////////////

function appConfig(mount, options, activateDevelopment) {
  var root = computeRootAppPath(mount);
  var path = transformMountToPath(mount);

  var file = fs.join(root, path, 'manifest.json');
   return {
    id: mount,
    path: path,
    options: options || {},
    mount: mount,
    manifest: validateManifestFile(file),
    isSystem: isSystemMount(mount),
    isDevelopment: activateDevelopment || false
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Creates an app with options and returns it
/// All errors are handled including app not found. Returns undefined if app is invalid.
/// If the app is valid it will be added into the local app cache.
////////////////////////////////////////////////////////////////////////////////

function createApp(mount, options, activateDevelopment) {
  var dbname = arangodb.db._name();
  var config = appConfig(mount, options, activateDevelopment);
  var app = new ArangoApp(config);
  appCache[dbname][mount] = app;
  return app;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Distributes zip file to peer coordinators. Only used in cluster
////////////////////////////////////////////////////////////////////////////////

function uploadToPeerCoordinators(appInfo, coordinators) {
  let coordOptions = {
    coordTransactionID: ArangoClusterInfo.uniqid()
  };
  let req = fs.readBuffer(fs.join(fs.getTempPath(), appInfo));
  console.log(appInfo, req);
  let httpOptions = {};
  let mapping = {};
  for (let i = 0; i < coordinators.length; ++i) {
    let ctid = ArangoClusterInfo.uniqid();
    mapping[ctid] = coordinators[i];
    coordOptions.clientTransactionID = ctid;
    ArangoClusterComm.asyncRequest('POST', 'server:' + coordinators[i], db._name(),
      '/_api/upload', req, httpOptions, coordOptions);
  }
  return {
    results: cluster.wait(coordOptions, coordinators),
    mapping: mapping
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Generates an App with the given options into the targetPath
////////////////////////////////////////////////////////////////////////////////

function installAppFromGenerator(targetPath, options) {
  var invalidOptions = [];
  // Set default values:
  options.name = options.name || 'MyApp';
  options.author = options.author || 'Author';
  options.description = options.description || '';
  options.license = options.license || 'Apache 2';
  options.authenticated = options.authenticated || false;
  options.collectionNames = options.collectionNames || [];
  if (typeof options.name !== 'string') {
    invalidOptions.push('options.name has to be a string.');
  }
  if (typeof options.author !== 'string') {
    invalidOptions.push('options.author has to be a string.');
  }
  if (typeof options.description !== 'string') {
    invalidOptions.push('options.description has to be a string.');
  }
  if (typeof options.license !== 'string') {
    invalidOptions.push('options.license has to be a string.');
  }
  if (typeof options.authenticated !== 'boolean') {
    invalidOptions.push('options.authenticated has to be a boolean.');
  }
  if (!Array.isArray(options.collectionNames)) {
    invalidOptions.push('options.collectionNames has to be an array.');
  }
  if (invalidOptions.length > 0) {
    console.log(invalidOptions);
    throw new ArangoError({
      errorNum: errors.ERROR_INVALID_FOXX_OPTIONS.code,
      errorMessage: JSON.stringify(invalidOptions, undefined, 2)
    });
  }
  options.path = targetPath;
  var engine = new TemplateEngine(options);
  engine.write();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Extracts an app from zip and moves it to temporary path
///
/// return path to app
////////////////////////////////////////////////////////////////////////////////

function extractAppToPath(archive, targetPath, noDelete) {
  var tempFile = fs.getTempFile('zip', false);
  fs.makeDirectory(tempFile);
  fs.unzipFile(archive, tempFile, false, true);

  // .............................................................................
  // throw away source file
  // .............................................................................
  if (!noDelete) {
    try {
      fs.remove(archive);
    }
    catch (err1) {
      arangodb.printf(`Cannot remove temporary file "${archive}"\n`);
    }
  }

  // .............................................................................
  // locate the manifest file
  // .............................................................................

  var tree = fs.listTree(tempFile).sort(function(a, b) {
    return a.length - b.length;
  });
  var found;
  var mf = 'manifest.json';
  var re = /[\/\\\\]manifest\.json$/; // Windows!
  var tf;
  var i;

  for (i = 0; i < tree.length && found === undefined; ++i) {
    tf = tree[i];

    if (re.test(tf) || tf === mf) {
      found = tf;
    }
  }

  if (found === undefined) {
    throwFileNotFound(`Cannot find manifest file in zip file "${tempFile}"`);
  }

  var mp;

  if (found === mf) {
    mp = '.';
  }
  else {
    mp = found.substr(0, found.length - mf.length - 1);
  }

  fs.move(fs.join(tempFile, mp), targetPath);

  // .............................................................................
  // throw away temporary app folder
  // .............................................................................
  if (found !== mf) {
    try {
      fs.removeDirectoryRecursive(tempFile);
    }
    catch (err1) {
      let details = String(err1.stack || err1);
      arangodb.printf(`Cannot remove temporary folder "${tempFile}"\n Stack: ${details}`);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds a github repository URL
////////////////////////////////////////////////////////////////////////////////

function buildGithubUrl(appInfo) {
  var splitted = appInfo.split(':');
  var repository = splitted[1];
  var version = splitted[2];
  if (version === undefined) {
    version = 'master';
  }

  var urlPrefix = require('process').env.FOXX_BASE_URL;
  if (urlPrefix === undefined) {
    urlPrefix = 'https://github.com/';
  }
  return urlPrefix + repository + '/archive/' + version + '.zip';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Downloads an app from remote zip file and copies it to mount path
////////////////////////////////////////////////////////////////////////////////

function installAppFromRemote(url, targetPath) {
  var tempFile = fs.getTempFile('downloads', false);
  var auth;

  var urlObj = parseUrl(url);
  if (urlObj.auth) {
    require('console').log('old path', url);
    auth = urlObj.auth.split(':');
    auth = {
      username: decodeURIComponent(auth[0]),
      password: decodeURIComponent(auth[1])
    };
    delete urlObj.auth;
    url = formatUrl(urlObj);
    require('console').log('new path', url);
  }

  try {
    var result = download(url, '', {
      method: 'get',
      followRedirects: true,
      timeout: 30,
      auth: auth
    }, tempFile);

    if (result.code < 200 || result.code > 299) {
      throwDownloadError(`Could not download from "${url}"`);
    }
  }
  catch (err) {
    let details = String(err.stack || err);
    throwDownloadError(`Could not download from "${url}": ${details}`);
  }
  extractAppToPath(tempFile, targetPath);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Copies an app from local, either zip file or folder, to mount path
////////////////////////////////////////////////////////////////////////////////

function installAppFromLocal(path, targetPath) {
  if (fs.isDirectory(path)) {
    extractAppToPath(utils.zipDirectory(path), targetPath);
  } else {
    extractAppToPath(path, targetPath, true);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief run a Foxx application script
///
/// Input:
/// * scriptName: the script name
/// * mount: the mount path starting with a "/"
///
/// Output:
/// -
////////////////////////////////////////////////////////////////////////////////

function runScript(scriptName, mount, options) {
  checkParameter(
    'runScript(<scriptName>, <mount>, [<options>])',
    [ [ 'Script name', 'string' ], [ 'Mount path', 'string' ] ],
    [ scriptName, mount ]
  );

  var app = lookupApp(mount);

  return executeAppScript(scriptName, app, options) || null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the app's README.md
///
/// Input:
/// * mount: the mount path starting with a "/"
///
/// Output:
/// -
////////////////////////////////////////////////////////////////////////////////

function readme(mount) {
  checkParameter(
    'readme(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]
  );
  let app = lookupApp(mount);
  let path, readmeText;

  path = fs.join(app._root, app._path, 'README.md');
  readmeText = fs.exists(path) && fs.read(path);
  if (!readmeText) {
    path = fs.join(app._root, app._path, 'README');
    readmeText = fs.exists(path) && fs.read(path);
  }
  return readmeText || null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run a Foxx application's tests
///
/// Input:
/// * mount: the mount path starting with a "/"
///
/// Output:
/// -
////////////////////////////////////////////////////////////////////////////////

function runTests(mount, options) {
  checkParameter(
    'runTests(<mount>, [<options>])',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]
  );

  var app = lookupApp(mount);
  var reporter = options ? options.reporter : null;
  return require('org/arangodb/foxx/mocha').run(app, reporter);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Initializes the appCache and fills it initially for each db.
////////////////////////////////////////////////////////////////////////////////

function initCache() {
  var dbname = arangodb.db._name();
  if (!appCache.hasOwnProperty(dbname)) {
    initializeFoxx();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Internal scanFoxx function. Check scanFoxx.
/// Does not check parameters and throws errors.
////////////////////////////////////////////////////////////////////////////////

function _scanFoxx(mount, options, activateDevelopment) {
  options = options || { };
  var dbname = arangodb.db._name();
  delete appCache[dbname][mount];
  var app = createApp(mount, options, activateDevelopment);
  if (!options.__clusterDistribution) {
    try {
      utils.getStorage().save(app.toJSON());
    }
    catch (err) {
      if (!options.replace || err.errorNum !== errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code) {
        throw err;
      }
      var old = utils.getStorage().firstExample({ mount: mount });
      if (old === null) {
        throw new Error(`Could not find app for mountpoint "${mount}"`);
      }
      var manifest = app.toJSON().manifest;
      utils.getStorage().update(old, {manifest: manifest});
    }
  }
  return app;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Scans the sources of the given mountpoint and publishes the routes
///
/// TODO: Long Documentation!
////////////////////////////////////////////////////////////////////////////////

function scanFoxx(mount, options) {
  checkParameter(
    'scanFoxx(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ] );
  initCache();
  var app = _scanFoxx(mount, options);
  reloadRouting();
  return app.simpleJSON();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Scans the sources of the given mountpoint and publishes the routes
///
/// TODO: Long Documentation!
////////////////////////////////////////////////////////////////////////////////

function rescanFoxx(mount) {
  checkParameter(
    'scanFoxx(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ] );

  var old = lookupApp(mount);
  var collection = utils.getStorage();
  initCache();
  db._executeTransaction({
    collections: {
      write: collection.name()
    },
    action() {
      var definition = collection.firstExample({mount: mount});
      if (definition !== null) {
        collection.remove(definition._key);
      }
      _scanFoxx(mount, old._options, old._isDevelopment);
    }
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Build app in path
////////////////////////////////////////////////////////////////////////////////

function _buildAppInPath(appInfo, path, options) {
  try {
    if (appInfo === 'EMPTY') {
      // Make Empty app
      installAppFromGenerator(path, options || {});
    } else if (/^GIT:/i.test(appInfo)) {
      installAppFromRemote(buildGithubUrl(appInfo), path);
    } else if (/^https?:/i.test(appInfo)) {
      installAppFromRemote(appInfo, path);
    } else if (utils.pathRegex.test(appInfo)) {
      installAppFromLocal(appInfo, path);
    } else if (/^uploads[\/\\]tmp-/.test(appInfo)) {
      appInfo = fs.join(fs.getTempPath(), appInfo);
      installAppFromLocal(appInfo, path);
    } else {
      installAppFromRemote(store.buildUrl(appInfo), path);
    }
  } catch (e) {
    try {
      fs.removeDirectoryRecursive(path, true);
    } catch (err) {
      // noop
    }
    throw e;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Internal app validation function
/// Does not check parameters and throws errors.
////////////////////////////////////////////////////////////////////////////////

function _validateApp(appInfo) {
  var tempPath = fs.getTempFile('apps', false);
  try {
    _buildAppInPath(appInfo, tempPath, {});
    var tmp = new ArangoApp(fakeAppConfig(tempPath));
    if (!tmp.needsConfiguration()) {
      routeApp(tmp, true);
      exportApp(tmp);
    }
  } catch (e) {
    throw e;
  } finally {
    fs.removeDirectoryRecursive(tempPath, true);
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Internal install function. Check install.
/// Does not check parameters and throws errors.
////////////////////////////////////////////////////////////////////////////////

function _install(appInfo, mount, options, runSetup) {
  var targetPath = computeAppPath(mount, true);
  var app;
  var collection = utils.getStorage();
  options = options || {};
  if (fs.exists(targetPath)) {
    throw new Error('An app is already installed at this location.');
  }
  fs.makeDirectoryRecursive(targetPath);
  // Remove the empty APP folder.
  // Ohterwise move will fail.
  fs.removeDirectory(targetPath);

  initCache();
  _buildAppInPath(appInfo, targetPath, options);
  try {
    db._executeTransaction({
      collections: {
        write: collection.name()
      },
      action() {
        app = _scanFoxx(mount, options);
      }
    });
    if (runSetup) {
      executeAppScript('setup', lookupApp(mount));
    }
    if (!app.needsConfiguration()) {
      // Validate Routing
      routeApp(app, true);
      // Validate Exports
      exportApp(app);
    }
  } catch (e) {
    try {
      fs.removeDirectoryRecursive(targetPath, true);
    } catch (err) {
      console.errorLines(err.stack);
    }
    try {
      if (!options.__clusterDistribution) {
        db._executeTransaction({
          collections: {
            write: collection.name()
          },
          action() {
            var definition = collection.firstExample({mount: mount});
            collection.remove(definition._key);
          }
        });
      }
    } catch (err) {
      console.errorLines(err.stack);
    }
    if (e instanceof ArangoError) {
      if (e.errorNum === errors.ERROR_MODULE_SYNTAX_ERROR.code) {
        throw _.extend(new ArangoError({
          errorNum: errors.ERROR_SYNTAX_ERROR_IN_SCRIPT.code,
          errorMessage: errors.ERROR_SYNTAX_ERROR_IN_SCRIPT.message
        }), {stack: e.stack});
      }
      if (e.errorNum === errors.ERROR_MODULE_FAILURE.code) {
        throw _.extend(new ArangoError({
          errorNum: errors.ERROR_FAILED_TO_EXECUTE_SCRIPT.code,
          errorMessage: errors.ERROR_FAILED_TO_EXECUTE_SCRIPT.message
        }), {stack: e.stack});
      }
    }
    throw e;
  }
  return app;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Installs a new foxx application on the given mount point.
///
/// TODO: Long Documentation!
////////////////////////////////////////////////////////////////////////////////

function install(appInfo, mount, options) {
  checkParameter(
    'install(<appInfo>, <mount>, [<options>])',
    [ [ 'Install information', 'string' ],
      [ 'Mount path', 'string' ] ],
    [ appInfo, mount ] );
  utils.validateMount(mount);
  let hasToBeDistributed = /^uploads[\/\\]tmp-/.test(appInfo);
  var app = _install(appInfo, mount, options, true);
  options = options || {};
  if (ArangoServerState.isCoordinator() && !options.__clusterDistribution) {
    let name = ArangoServerState.id();
    let coordinators = ArangoClusterInfo.getCoordinators().filter(function(c) {
      return c !== name;
    });
    if (hasToBeDistributed) {
      let result = uploadToPeerCoordinators(appInfo, coordinators);
      let mapping = result.mapping;
      let res = result.results;
      let intOpts = JSON.parse(JSON.stringify(options));
      intOpts.__clusterDistribution = true;
      let coordOptions = {
        coordTransactionID: ArangoClusterInfo.uniqid()
      };
      let httpOptions = {};
      for (let i = 0; i < res.length; ++i) {
        let b = JSON.parse(res[i].body);
        /*jshint -W075:true */
        let intReq = {appInfo: b.filename, mount, options: intOpts};
        /*jshint -W075:false */
        ArangoClusterComm.asyncRequest('POST', 'server:' + mapping[res[i].clientTransactionID], db._name(),
          '/_admin/foxx/install', JSON.stringify(intReq), httpOptions, coordOptions);
      }
      cluster.wait(coordOptions, coordinators);
    } else {
      /*jshint -W075:true */
      let req = {appInfo, mount, options};
      /*jshint -W075:false */
      let httpOptions = {};
      let coordOptions = {
        coordTransactionID: ArangoClusterInfo.uniqid()
      };
      req.options.__clusterDistribution = true;
      req = JSON.stringify(req);
      for (let i = 0; i < coordinators.length; ++i) {
        if (coordinators[i] !== ArangoServerState.id()) {
          ArangoClusterComm.asyncRequest('POST', 'server:' + coordinators[i], db._name(),
            '/_admin/foxx/install', req, httpOptions, coordOptions);
        }
      }
    }
  }
  reloadRouting();
  return app.simpleJSON();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Internal install function. Check install.
/// Does not check parameters and throws errors.
////////////////////////////////////////////////////////////////////////////////

function _uninstall(mount, options) {
  var dbname = arangodb.db._name();
  if (!appCache.hasOwnProperty(dbname)) {
    initializeFoxx(options);
  }
  var app;
  options = options || {};
  try {
    app = lookupApp(mount);
  } catch (e) {
    if (!options.force) {
      throw e;
    }
  }
  var collection = utils.getStorage();
  var targetPath = computeAppPath(mount, true);
  if (!fs.exists(targetPath) && !options.force) {
    throw new ArangoError({
      errorNum: errors.ERROR_NO_FOXX_FOUND.code,
      errorMessage: errors.ERROR_NO_FOXX_FOUND.message
    });
  }
  delete appCache[dbname][mount];
  if (!options.__clusterDistribution) {
    try {
      db._executeTransaction({
        collections: {
          write: collection.name()
        },
        action() {
          var definition = collection.firstExample({mount: mount});
          collection.remove(definition._key);
        }
      });
    } catch (e) {
      if (!options.force) {
        throw e;
      }
    }
  }
  if (options.teardown !== false && options.teardown !== 'false') {
    try {
      executeAppScript('teardown', app);
    } catch (e) {
      if (!options.force) {
        throw e;
      }
    }
  }
  try {
    fs.removeDirectoryRecursive(targetPath, true);
  } catch (e) {
    if (!options.force) {
      throw e;
    }
  }
  if (options.force && app === undefined) {
    return {
      simpleJSON() {
        return {
          name: 'force uninstalled',
          version: 'unknown',
          mount: mount
        };
      }
    };
  }
  return app;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Uninstalls the foxx application on the given mount point.
///
/// TODO: Long Documentation!
////////////////////////////////////////////////////////////////////////////////

function uninstall(mount, options) {
  checkParameter(
    'uninstall(<mount>, [<options>])',
    [ [ 'Mount path', 'string' ] ],
    [ mount ] );
  utils.validateMount(mount);
  options = options || {};
  var app = _uninstall(mount, options);
  if (ArangoServerState.isCoordinator() && !options.__clusterDistribution) {
    let coordinators = ArangoClusterInfo.getCoordinators();
    /*jshint -W075:true */
    let req = {mount, options};
    /*jshint -W075:false */
    let httpOptions = {};
    let coordOptions = {
      coordTransactionID: ArangoClusterInfo.uniqid()
    };
    req.options.__clusterDistribution = true;
    req.options.force = true;
    req = JSON.stringify(req);
    for (let i = 0; i < coordinators.length; ++i) {
      if (coordinators[i] !== ArangoServerState.id()) {
        ArangoClusterComm.asyncRequest('POST', 'server:' + coordinators[i], db._name(),
          '/_admin/foxx/uninstall', req, httpOptions, coordOptions);
      }
    }
  }
  reloadRouting();
  return app.simpleJSON();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Replaces a foxx application on the given mount point by an other one.
///
/// TODO: Long Documentation!
////////////////////////////////////////////////////////////////////////////////

function replace(appInfo, mount, options) {
  checkParameter(
    'replace(<appInfo>, <mount>, [<options>])',
    [ [ 'Install information', 'string' ],
      [ 'Mount path', 'string' ] ],
    [ appInfo, mount ] );
  utils.validateMount(mount);
  _validateApp(appInfo);
  options = options || {};
  let hasToBeDistributed = /^uploads[\/\\]tmp-/.test(appInfo);
  if (ArangoServerState.isCoordinator() && !options.__clusterDistribution) {
    let name = ArangoServerState.id();
    let coordinators = ArangoClusterInfo.getCoordinators().filter(function(c) {
      return c !== name;
    });
    if (hasToBeDistributed) {
      let result = uploadToPeerCoordinators(appInfo, coordinators);
      let mapping = result.mapping;
      let res = result.results;
      let intOpts = JSON.parse(JSON.stringify(options));
      intOpts.__clusterDistribution = true;
      let coordOptions = {
        coordTransactionID: ArangoClusterInfo.uniqid()
      };
      let httpOptions = {};
      for (let i = 0; i < res.length; ++i) {
        let b = JSON.parse(res[i].body);
        /*jshint -W075:true */
        let intReq = {appInfo: b.filename, mount, options: intOpts};
        /*jshint -W075:false */
        ArangoClusterComm.asyncRequest('POST', 'server:' + mapping[res[i].coordinatorTransactionID], db._name(),
          '/_admin/foxx/replace', JSON.stringify(intReq), httpOptions, coordOptions);
      }
      cluster.wait(coordOptions, coordinators);
    } else {
      let intOpts = JSON.parse(JSON.stringify(options));
      /*jshint -W075:true */
      let req = {appInfo, mount, options: intOpts};
      /*jshint -W075:false */
      let httpOptions = {};
      let coordOptions = {
        coordTransactionID: ArangoClusterInfo.uniqid()
      };
      req.options.__clusterDistribution = true;
      req.options.force = true;
      req = JSON.stringify(req);
      for (let i = 0; i < coordinators.length; ++i) {
        ArangoClusterComm.asyncRequest('POST', 'server:' + coordinators[i], db._name(),
          '/_admin/foxx/replace', req, httpOptions, coordOptions);
      }
    }
  }
  _uninstall(mount, {teardown: true,
    __clusterDistribution: options.__clusterDistribution || false,
    force: !options.__clusterDistribution
  });
  var app = _install(appInfo, mount, options, true);
  reloadRouting();
  return app.simpleJSON();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Upgrade a foxx application on the given mount point by a new one.
///
/// TODO: Long Documentation!
////////////////////////////////////////////////////////////////////////////////

function upgrade(appInfo, mount, options) {
  checkParameter(
    'upgrade(<appInfo>, <mount>, [<options>])',
    [ [ 'Install information', 'string' ],
      [ 'Mount path', 'string' ] ],
    [ appInfo, mount ] );
  utils.validateMount(mount);
  _validateApp(appInfo);
  options = options || {};
  let hasToBeDistributed = /^uploads[\/\\]tmp-/.test(appInfo);
  if (ArangoServerState.isCoordinator() && !options.__clusterDistribution) {
    let name = ArangoServerState.id();
    let coordinators = ArangoClusterInfo.getCoordinators().filter(function(c) {
      return c !== name;
    });
    if (hasToBeDistributed) {
      let result = uploadToPeerCoordinators(appInfo, coordinators);
      let mapping = result.mapping;
      let res = result.results;
      let intOpts = JSON.parse(JSON.stringify(options));
      intOpts.__clusterDistribution = true;
      let coordOptions = {
        coordTransactionID: ArangoClusterInfo.uniqid()
      };
      let httpOptions = {};
      for (let i = 0; i < res.length; ++i) {
        let b = JSON.parse(res[i].body);
        /*jshint -W075:true */
        let intReq = {appInfo: b.filename, mount, options: intOpts};
        /*jshint -W075:false */
        ArangoClusterComm.asyncRequest('POST', 'server:' + mapping[res[i].coordinatorTransactionID], db._name(),
          '/_admin/foxx/update', JSON.stringify(intReq), httpOptions, coordOptions);
      }
      cluster.wait(coordOptions, coordinators);
    } else {
      let intOpts = JSON.parse(JSON.stringify(options));
      /*jshint -W075:true */
      let req = {appInfo, mount, options: intOpts};
      /*jshint -W075:false */
      let httpOptions = {};
      let coordOptions = {
        coordTransactionID: ArangoClusterInfo.uniqid()
      };
      req.options.__clusterDistribution = true;
      req.options.force = true;
      req = JSON.stringify(req);
      for (let i = 0; i < coordinators.length; ++i) {
        ArangoClusterComm.asyncRequest('POST', 'server:' + coordinators[i], db._name(),
          '/_admin/foxx/update', req, httpOptions, coordOptions);
      }
    }
  }
  var oldApp = lookupApp(mount);
  var oldConf = oldApp.getConfiguration(true);
  options.configuration = options.configuration || {};
  Object.keys(oldConf).forEach(function (key) {
    if (!options.configuration.hasOwnProperty(key)) {
      options.configuration[key] = oldConf[key];
    }
  });
  var oldDeps = oldApp._options.dependencies || {};
  options.dependencies = options.dependencies || {};
  Object.keys(oldDeps).forEach(function (key) {
    if (!options.dependencies.hasOwnProperty(key)) {
      options.dependencies[key] = oldDeps[key];
    }
  });
  _uninstall(mount, {teardown: false,
    __clusterDistribution: options.__clusterDistribution || false,
    force: !options.__clusterDistribution
  });
  var app = _install(appInfo, mount, options, true);
  reloadRouting();
  return app.simpleJSON();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the Foxx apps
////////////////////////////////////////////////////////////////////////////////

function initializeFoxx(options) {
  var dbname = arangodb.db._name();
  var mounts = syncWithFolder(options);
  refillCaches(dbname);
  checkMountedSystemApps(dbname);
  mounts.forEach(function (mount) {
    executeAppScript('setup', lookupApp(mount));
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compute all app routes
////////////////////////////////////////////////////////////////////////////////

function mountPoints() {
  var dbname = arangodb.db._name();
  return refillCaches(dbname);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief toggles development mode of app and reloads routing
////////////////////////////////////////////////////////////////////////////////

function _toggleDevelopment(mount, activate) {
  var app = lookupApp(mount);
  app.development(activate);
  utils.updateApp(mount, app.toJSON());
  reloadRouting();
  return app;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief activate development mode
////////////////////////////////////////////////////////////////////////////////

function setDevelopment(mount) {
  checkParameter(
    'development(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ] );
  var app = _toggleDevelopment(mount, true);
  return app.simpleJSON();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief activate production mode
////////////////////////////////////////////////////////////////////////////////

function setProduction(mount) {
  checkParameter(
    'production(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ] );
  var app = _toggleDevelopment(mount, false);
  return app.simpleJSON();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Configure the app at the mountpoint
////////////////////////////////////////////////////////////////////////////////

function configure(mount, options) {
  checkParameter(
    'configure(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ] );
  utils.validateMount(mount, true);
  var app = lookupApp(mount);
  var invalid = app.configure(options.configuration || {});
  if (invalid.length > 0) {
    // TODO Error handling
    console.log(invalid);
  }
  utils.updateApp(mount, app.toJSON());
  reloadRouting();
  return app.simpleJSON();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Set up dependencies of the app at the mountpoint
////////////////////////////////////////////////////////////////////////////////

function updateDeps(mount, options) {
  checkParameter(
    'updateDeps(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ] );
  utils.validateMount(mount, true);
  var app = lookupApp(mount);
  var invalid = app.updateDeps(options.dependencies || {});
  if (invalid.length > 0) {
    // TODO Error handling
    console.log(invalid);
  }
  utils.updateApp(mount, app.toJSON());
  reloadRouting();
  return app.simpleJSON();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the configuration for the app at the given mountpoint
////////////////////////////////////////////////////////////////////////////////

function configuration(mount) {
  checkParameter(
    'configuration(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ] );
  utils.validateMount(mount, true);
  var app = lookupApp(mount);
  return app.getConfiguration();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the dependencies for the app at the given mountpoint
////////////////////////////////////////////////////////////////////////////////

function dependencies(mount) {
  checkParameter(
    'dependencies(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ] );
  utils.validateMount(mount, true);
  var app = lookupApp(mount);
  return app.getDependencies();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Require the exports defined on the mount point
////////////////////////////////////////////////////////////////////////////////

function requireApp(mount) {
  checkParameter(
    'requireApp(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ] );
  utils.validateMount(mount, true);
  var app = lookupApp(mount);
  return exportApp(app);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Syncs the apps in ArangoDB with the applications stored on disc
////////////////////////////////////////////////////////////////////////////////

function syncWithFolder(options) {
  var dbname = arangodb.db._name();
  options = options || {};
  options.replace = true;
  appCache = appCache || {};
  appCache[dbname] = {};
  var folders = fs.listTree(Module._appPath).filter(filterAppRoots);
  var collection = utils.getStorage();
  return folders.map(function (folder) {
    var mount;
    db._executeTransaction({
      collections: {
        write: collection.name()
      },
      action() {
        mount = transformPathToMount(folder);
        _scanFoxx(mount, options);
      }
    });
    return mount;
  });
}

// -----------------------------------------------------------------------------
// --SECTION--                                                           exports
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Exports
////////////////////////////////////////////////////////////////////////////////

exports.syncWithFolder = syncWithFolder;
exports.install = install;
exports.readme = readme;
exports.runTests = runTests;
exports.runScript = runScript;
exports.setup = R.partial(runScript, 'setup');
exports.teardown = R.partial(runScript, 'teardown');
exports.uninstall = uninstall;
exports.replace = replace;
exports.upgrade = upgrade;
exports.development = setDevelopment;
exports.production = setProduction;
exports.configure = configure;
exports.updateDeps = updateDeps;
exports.configuration = configuration;
exports.dependencies = dependencies;
exports.requireApp = requireApp;
exports._resetCache = resetCache;

////////////////////////////////////////////////////////////////////////////////
/// @brief Serverside only API
////////////////////////////////////////////////////////////////////////////////

exports.scanFoxx = scanFoxx;
exports.mountPoints = mountPoints;
exports.routes = routes;
exports.rescanFoxx = rescanFoxx;
exports.lookupApp = lookupApp;

////////////////////////////////////////////////////////////////////////////////
/// @brief Exports from foxx utils module.
////////////////////////////////////////////////////////////////////////////////

exports.mountedApp = utils.mountedApp;
exports.list = utils.list;
exports.listJson = utils.listJson;
exports.listDevelopment = utils.listDevelopment;
exports.listDevelopmentJson = utils.listDevelopmentJson;

////////////////////////////////////////////////////////////////////////////////
/// @brief Exports from foxx store module.
////////////////////////////////////////////////////////////////////////////////

exports.available = store.available;
exports.availableJson = store.availableJson;
exports.getFishbowlStorage = store.getFishbowlStorage;
exports.search = store.search;
exports.searchJson = store.searchJson;
exports.update = store.update;
exports.info = store.info;

exports.initializeFoxx = initializeFoxx;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:

/* global ArangoServerState, ArangoClusterInfo, ArangoClusterComm */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Foxx service manager
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2013 triagens GmbH, Cologne, Germany
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Dr. Frank Celler
// / @author Michael Hackstein
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const fs = require('fs');
const joinPath = require('path').join;
const joi = require('joi');
const util = require('util');
const semver = require('semver');
const dd = require('dedent');
const il = require('@arangodb/util').inline;
const utils = require('@arangodb/foxx/manager-utils');
const store = require('@arangodb/foxx/store');
const FoxxService = require('@arangodb/foxx/service');
const generator = require('@arangodb/foxx/generator');
const routeAndExportService = require('@arangodb/foxx/routing').routeService;
const formatUrl = require('url').format;
const parseUrl = require('url').parse;
const arangodb = require('@arangodb');
const ArangoError = arangodb.ArangoError;
const db = arangodb.db;
const checkParameter = arangodb.checkParameter;
const errors = arangodb.errors;
const cluster = require('@arangodb/cluster');
const download = require('internal').download;
const executeGlobalContextFunction = require('internal').executeGlobalContextFunction;
const actions = require('@arangodb/actions');
const plainServerVersion = require('@arangodb').plainServerVersion;

const throwDownloadError = arangodb.throwDownloadError;
const throwFileNotFound = arangodb.throwFileNotFound;

// Regular expressions for joi patterns
const RE_EMPTY = /^$/;
const RE_NOT_EMPTY = /./;

const legacyManifestFields = [
  'assets',
  'controllers',
  'exports',
  'isSystem'
];

const manifestSchema = {
  // FoxxStore metadata
  name: joi.string().regex(/^[-_a-z][-_a-z0-9]*$/i).optional(),
  version: joi.string().optional(),
  keywords: joi.array().optional(),
  license: joi.string().optional(),
  repository: (
  joi.object().optional()
    .keys({
      type: joi.string().required(),
      url: joi.string().required()
    })
  ),

  // Additional web interface metadata
  author: joi.string().allow('').default(''),
  contributors: joi.array().optional(),
  description: joi.string().allow('').default(''),
  thumbnail: joi.string().optional(),

  // Compatibility
  engines: (
  joi.object().optional()
    .pattern(RE_EMPTY, joi.forbidden())
    .pattern(RE_NOT_EMPTY, joi.string().required())
  ),

  // Index redirect
  defaultDocument: joi.string().allow('').optional(),

  // JS path
  lib: joi.string().default('.'),

  // Entrypoint
  main: joi.string().optional(),

  // Config
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

  // Dependencies supported
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

  // Dependencies provided
  provides: (
  joi.alternatives().try(
    joi.string().optional(),
    joi.array().optional()
      .items(joi.string().required()),
    joi.object().optional()
      .pattern(RE_EMPTY, joi.forbidden())
      .pattern(RE_NOT_EMPTY, joi.string().required())
  )
  ),

  // Bundled assets
  files: (
  joi.object().optional()
    .pattern(RE_EMPTY, joi.forbidden())
    .pattern(RE_NOT_EMPTY, joi.alternatives().try(
      joi.string().required(),
      joi.object().required()
        .keys({
          path: joi.string().required(),
          gzip: joi.boolean().optional(),
          type: joi.string().optional()
        })
    ))
  ),

  // Scripts/queue jobs
  scripts: (
  joi.object().optional()
    .pattern(RE_EMPTY, joi.forbidden())
    .pattern(RE_NOT_EMPTY, joi.string().required())
    .default(Object, 'empty scripts object')
  ),

  // Foxx tests path
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
  )
};

var serviceCache = {};
var usedSystemMountPoints = [
  '/_admin/aardvark', // Admin interface.
  '/_api/gharial' // General_Graph API.
];

// //////////////////////////////////////////////////////////////////////////////
// / @brief Searches through a tree of files and returns true for all service roots
// //////////////////////////////////////////////////////////////////////////////

function filterServiceRoots (folder) {
  return /[\\\/]APP$/i.test(folder) && !/(APP[\\\/])(.*)APP$/i.test(folder);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Trigger reload routing
// / Triggers reloading of routes in this as well as all other threads.
// //////////////////////////////////////////////////////////////////////////////

function reloadRouting () {
  executeGlobalContextFunction('reloadRouting');
  actions.reloadRouting();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Resets the service cache
// //////////////////////////////////////////////////////////////////////////////

function resetCache () {
  _.each(serviceCache, (cache) => {
    _.each(cache, (service) => {
      service.main.loaded = false;
    });
  });
  serviceCache = {};
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief lookup service in cache
// / Returns either the service or undefined if it is not cached.
// //////////////////////////////////////////////////////////////////////////////

function lookupService (mount) {
  var dbname = arangodb.db._name();
  if (!serviceCache.hasOwnProperty(dbname) || Object.keys(serviceCache[dbname]).length === 0) {
    refillCaches(dbname);
  }
  if (!serviceCache[dbname].hasOwnProperty(mount)) {
    refillCaches(dbname);
    if (serviceCache[dbname].hasOwnProperty(mount)) {
      return serviceCache[dbname][mount];
    }
    throw new ArangoError({
      errorNum: errors.ERROR_SERVICE_NOT_FOUND.code,
      errorMessage: dd`
        ${errors.ERROR_SERVICE_NOT_FOUND.message}
        Mount path: "${mount}".
      `
    });
  }
  return serviceCache[dbname][mount];
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief refills the routing cache
// //////////////////////////////////////////////////////////////////////////////

function refillCaches (dbname) {
  var cache = {};
  _.each(serviceCache[dbname], (service) => {
    service.main.loaded = false;
  });

  var cursor = utils.getStorage().all();
  var routes = [];

  while (cursor.hasNext()) {
    var config = cursor.next();
    var service = new FoxxService(Object.assign({}, config));
    var mount = service.mount;
    cache[mount] = service;
    routes.push(mount);
  }

  serviceCache[dbname] = cache;
  return routes;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief routes of a foxx
// //////////////////////////////////////////////////////////////////////////////

function routes (mount) {
  var service = lookupService(mount);
  return routeAndExportService(service, false).routes;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief ensure a foxx is routed
// //////////////////////////////////////////////////////////////////////////////

function ensureRouted (mount) {
  var service = lookupService(mount);
  return routeAndExportService(service, false);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Makes sure all system services are mounted.
// //////////////////////////////////////////////////////////////////////////////

function checkMountedSystemService (dbname) {
  var i, mount;
  // var collection = utils.getStorage()
  for (i = 0; i < usedSystemMountPoints.length; ++i) {
    mount = usedSystemMountPoints[i];
    delete serviceCache[dbname][mount];
    _scanFoxx(mount, {replace: true});
    lookupService(mount).executeScript('setup');
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief check a manifest for completeness
// //////////////////////////////////////////////////////////////////////////////

function checkManifest (filename, inputManifest, mount, isDevelopment) {
  const serverVersion = plainServerVersion();
  const errors = [];
  const manifest = {};
  let legacy = false;

  Object.keys(manifestSchema).forEach(function (key) {
    const schema = manifestSchema[key];
    const value = inputManifest[key];
    const result = joi.validate(value, schema);
    if (result.error) {
      const error = result.error.message.replace(/^"value"/, `Value`);
      errors.push(il`
        Service at "${mount}" specifies manifest field "${key}"
        with invalid value "${util.format(value)}":
        ${error}
      `);
    } else {
      manifest[key] = result.value;
    }
  });

  if (manifest.engines && manifest.engines.arangodb) {
    if (semver.gtr('3.0.0', manifest.engines.arangodb)) {
      legacy = true;
      if (!isDevelopment) {
        console.infoLines(il`
          Service at "${mount}" expects version "${manifest.engines.arangodb}"
          and will run in legacy compatibility mode.
        `);
      }
    } else if (!semver.satisfies(serverVersion, manifest.engines.arangodb)) {
      if (!isDevelopment) {
        console.warnLines(il`
          Service at "${mount}" expects version "${manifest.engines.arangodb}"
          which is likely incompatible with installed version "${serverVersion}".
        `);
      }
    }
  }

  for (const key of Object.keys(inputManifest)) {
    if (manifestSchema[key]) {
      continue;
    }
    manifest[key] = inputManifest[key];
    if (key === 'engine' && !inputManifest.engines) {
      console.warnLines(il`
        Service at "${mount}" specifies unknown manifest field "engine".
        Did you mean "engines"?
      `);
    } else if (!legacy || legacyManifestFields.indexOf(key) === -1) {
      console.warnLines(il`
        Service at "${mount}" specifies unknown manifest field "${key}".
      `);
    }
  }

  if (manifest.version && !semver.valid(manifest.version)) {
    console.warnLines(il`
      Service at "${mount}" specifies manifest field "version"
      with invalid value "${manifest.version}".
    `);
  }

  if (manifest.provides) {
    if (typeof manifest.provides === 'string') {
      manifest.provides = [manifest.provides];
    }
    if (Array.isArray(manifest.provides)) {
      const provides = manifest.provides;
      manifest.provides = {};
      for (const provided of provides) {
        const tokens = provided.split(':');
        manifest.provides[tokens[0]] = tokens[1] || '*';
      }
    }
    for (const name of Object.keys(manifest.provides)) {
      const version = manifest.provides[name];
      if (!semver.valid(version)) {
        errors.push(il`
          Service at "${mount}" specifies manifest field "provides"
          with "${name}" set to invalid value "${version}".
        `);
      }
    }
  }

  if (manifest.dependencies) {
    for (const key of Object.keys(manifest.dependencies)) {
      if (typeof manifest.dependencies[key] === 'string') {
        const tokens = manifest.dependencies[key].split(':');
        manifest.dependencies[key] = {
          name: tokens[0] || '*',
          version: tokens[1] || '*',
          required: true
        };
      }
      const version = manifest.dependencies[key].version;
      if (!semver.validRange(version)) {
        errors.push(il`
          Service at "${mount}" specifies manifest field "dependencies"
          with "${key}" set to invalid value "${version}".
        `);
      }
    }
  }

  if (errors.length) {
    for (const error of errors) {
      console.errorLines(error);
    }
    throw new ArangoError({
      errorNum: errors.ERROR_INVALID_SERVICE_MANIFEST.code,
      errorMessage: dd`
        ${errors.ERROR_INVALID_SERVICE_MANIFEST.message}
        Manifest for service at "${mount}":
        ${errors.join('\n')}
      `
    });
  }

  if (legacy) {
    if (manifest.defaultDocument === undefined) {
      manifest.defaultDocument = 'index.html';
    }

    if (typeof manifest.controllers === 'string') {
      manifest.controllers = {'/': manifest.controllers};
    }
  }

  if (typeof manifest.tests === 'string') {
    manifest.tests = [manifest.tests];
  }

  return manifest;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief validates a manifest file and returns it.
// / All errors are handled including file not found. Returns undefined if manifest is invalid
// //////////////////////////////////////////////////////////////////////////////

function validateManifestFile (filename, mount, isDevelopment) {
  let mf;
  if (!fs.exists(filename)) {
    throwFileNotFound(`Cannot find manifest file "${filename}"`);
  }
  try {
    mf = JSON.parse(fs.read(filename));
  } catch (e) {
    throw Object.assign(
      new ArangoError({
        errorNum: errors.ERROR_MALFORMED_MANIFEST_FILE.code,
        errorMessage: dd`
          ${errors.ERROR_MALFORMED_MANIFEST_FILE.message}
          File: ${filename}
          Cause: ${e.stack}
        `
      }), {cause: e}
    );
  }
  try {
    mf = checkManifest(filename, mf, mount, isDevelopment);
  } catch (e) {
    throw Object.assign(
      new ArangoError({
        errorNum: errors.ERROR_INVALID_SERVICE_MANIFEST.code,
        errorMessage: dd`
          ${errors.ERROR_INVALID_SERVICE_MANIFEST.message}
          File: ${filename}
          Cause: ${e.stack}
        `
      }), {cause: e}
    );
  }
  return mf;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Checks if the mountpoint is reserved for system services
// //////////////////////////////////////////////////////////////////////////////

function isSystemMount (mount) {
  return (/^\/_/).test(mount);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the root path for service. Knows about system services
// //////////////////////////////////////////////////////////////////////////////

function computeRootServicePath (mount) {
  if (isSystemMount(mount)) {
    return FoxxService._systemAppPath;
  }
  return FoxxService._appPath;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief transforms a mount point to a sub-path relative to root
// //////////////////////////////////////////////////////////////////////////////

function transformMountToPath (mount) {
  var list = mount.split('/');
  return joinPath(...list, 'APP');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief transforms a sub-path to a mount point
// //////////////////////////////////////////////////////////////////////////////

function transformPathToMount (path) {
  var list = path.split(fs.pathSeparator);
  list.pop();
  return '/' + list.join('/');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the service path for mount point
// //////////////////////////////////////////////////////////////////////////////

function computeServicePath (mount) {
  var root = computeRootServicePath(mount);
  var mountPath = transformMountToPath(mount);
  return joinPath(root, mountPath);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns a valid service config for validation purposes
// //////////////////////////////////////////////////////////////////////////////

function fakeServiceConfig (path, mount) {
  var file = joinPath(path, 'manifest.json');
  return {
    id: '__internal',
    root: '',
    path: path,
    options: {},
    mount: '/internal',
    manifest: validateManifestFile(file, mount),
    isSystem: false,
    isDevelopment: false
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the service path and manifest
// //////////////////////////////////////////////////////////////////////////////

function serviceConfig (mount, options, activateDevelopment) {
  var root = computeRootServicePath(mount);
  var path = transformMountToPath(mount);

  var file = joinPath(root, path, 'manifest.json');
  return {
    id: mount,
    path: path,
    options: options || {},
    mount: mount,
    manifest: validateManifestFile(file, mount, activateDevelopment),
    isSystem: isSystemMount(mount),
    isDevelopment: activateDevelopment || false
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Creates a service with options and returns it
// / All errors are handled including service not found. Returns undefined if service is invalid.
// / If the service is valid it will be added into the local service cache.
// //////////////////////////////////////////////////////////////////////////////

function createService (mount, options, activateDevelopment) {
  var dbname = arangodb.db._name();
  var config = serviceConfig(mount, options, activateDevelopment);
  var service = new FoxxService(config);
  serviceCache[dbname][mount] = service;
  return service;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Distributes zip file to peer coordinators. Only used in cluster
// //////////////////////////////////////////////////////////////////////////////

function uploadToPeerCoordinators (serviceInfo, coordinators) {
  let coordOptions = {
    coordTransactionID: ArangoClusterComm.getId()
  };
  let req = fs.readBuffer(joinPath(fs.getTempPath(), serviceInfo));
  let httpOptions = {};
  let mapping = {};
  for (let i = 0; i < coordinators.length; ++i) {
    let ctid = ArangoClusterInfo.uniqid();
    mapping[ctid] = coordinators[i];
    coordOptions.clientTransactionID = ctid;
    ArangoClusterComm.asyncRequest('POST', 'server:' + coordinators[i], db._name(),
      '/_api/upload', req, httpOptions, coordOptions);
  }
  delete coordOptions.clientTransactionID;
  return {
    results: cluster.wait(coordOptions, coordinators.length),
  mapping};
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Generates a service with the given options into the targetPath
// //////////////////////////////////////////////////////////////////////////////

function installServiceFromGenerator (targetPath, options) {
  var invalidOptions = [];
  // Set default values:
  options.documentCollections = options.documentCollections || [];
  options.edgeCollections = options.edgeCollections || [];
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
  if (!Array.isArray(options.documentCollections)) {
    invalidOptions.push('options.documentCollections has to be an array.');
  }
  if (!Array.isArray(options.edgeCollections)) {
    invalidOptions.push('options.edgeCollections has to be an array.');
  }
  if (invalidOptions.length > 0) {
    throw new ArangoError({
      errorNum: errors.ERROR_INVALID_FOXX_OPTIONS.code,
      errorMessage: dd`
        ${errors.ERROR_INVALID_FOXX_OPTIONS.message}
        Options: ${JSON.stringify(invalidOptions, undefined, 2)}
      `
    });
  }
  var cfg = generator.generate(options);
  generator.write(targetPath, cfg.files, cfg.folders);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Extracts a service from zip and moves it to temporary path
// /
// / return path to service
// //////////////////////////////////////////////////////////////////////////////

function extractServiceToPath (archive, targetPath, noDelete) {
  var tempFile = fs.getTempFile('zip', false);
  fs.makeDirectory(tempFile);
  fs.unzipFile(archive, tempFile, false, true);

  // .............................................................................
  // throw away source file
  // .............................................................................
  if (!noDelete) {
    try {
      fs.remove(archive);
    } catch (err1) {
      arangodb.printf(`Cannot remove temporary file "${archive}"\n`);
    }
  }

  // .............................................................................
  // locate the manifest file
  // .............................................................................

  var tree = fs.listTree(tempFile).sort(function (a, b) {
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
  } else {
    mp = found.substr(0, found.length - mf.length - 1);
  }

  fs.move(joinPath(tempFile, mp), targetPath);

  // .............................................................................
  // throw away temporary service folder
  // .............................................................................
  if (found !== mf) {
    try {
      fs.removeDirectoryRecursive(tempFile);
    } catch (err1) {
      let details = String(err1.stack || err1);
      arangodb.printf(`Cannot remove temporary folder "${tempFile}"\n Stack: ${details}`);
    }
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief builds a github repository URL
// //////////////////////////////////////////////////////////////////////////////

function buildGithubUrl (serviceInfo) {
  var splitted = serviceInfo.split(':');
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief Downloads a service from remote zip file and copies it to mount path
// //////////////////////////////////////////////////////////////////////////////

function installServiceFromRemote (url, targetPath) {
  var tempFile = fs.getTempFile('downloads', false);
  var auth;

  var urlObj = parseUrl(url);
  if (urlObj.auth) {
    auth = urlObj.auth.split(':');
    auth = {
      username: decodeURIComponent(auth[0]),
      password: decodeURIComponent(auth[1])
    };
    delete urlObj.auth;
    url = formatUrl(urlObj);
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
  } catch (err) {
    let details = String(err.stack || err);
    throwDownloadError(`Could not download from "${url}": ${details}`);
  }
  extractServiceToPath(tempFile, targetPath);
}

function patchManifestFile (servicePath, patchData) {
  if (!patchData || !Object.keys(patchData).length) {
    return;
  }
  const filename = joinPath(servicePath, 'manifest.json');
  let manifest;
  try {
    const rawManifest = fs.read(filename);
    manifest = JSON.parse(rawManifest);
  } catch (e) {
    throw Object.assign(
      new ArangoError({
        errorNum: errors.ERROR_MALFORMED_MANIFEST_FILE.code,
        errorMessage: dd`
          ${errors.ERROR_MALFORMED_MANIFEST_FILE.message}
          File: ${filename}
          Cause: ${e.stack}
        `
      }), {cause: e}
    );
  }
  Object.assign(manifest, patchData);
  fs.write(filename, JSON.stringify(manifest, null, 2));
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Copies a service from local, either zip file or folder, to mount path
// //////////////////////////////////////////////////////////////////////////////

function installServiceFromLocal (path, targetPath) {
  if (fs.isDirectory(path)) {
    extractServiceToPath(utils.zipDirectory(path), targetPath);
  } else {
    extractServiceToPath(path, targetPath, true);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief run a Foxx service script
// /
// / Input:
// / * scriptName: the script name
// / * mount: the mount path starting with a "/"
// /
// / Output:
// / -
// //////////////////////////////////////////////////////////////////////////////

function runScript (scriptName, mount, options) {
  checkParameter(
    'runScript(<scriptName>, <mount>, [<options>])',
    [ [ 'Script name', 'string' ], [ 'Mount path', 'string' ] ],
    [ scriptName, mount ]
  );

  var service = lookupService(mount);
  if (service.isDevelopment && scriptName !== 'setup') {
    service.executeScript('setup');
  }
  ensureRouted(mount);
  return service.executeScript(scriptName, options) || null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the service's README.md
// /
// / Input:
// / * mount: the mount path starting with a "/"
// /
// / Output:
// / -
// //////////////////////////////////////////////////////////////////////////////

function readme (mount) {
  checkParameter(
    'readme(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]
  );
  let service = lookupService(mount);
  let path, readmeText;

  path = joinPath(service.root, service.path, 'README.md');
  readmeText = fs.exists(path) && fs.read(path);
  if (!readmeText) {
    path = joinPath(service.root, service.path, 'README');
    readmeText = fs.exists(path) && fs.read(path);
  }
  return readmeText || null;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief run a Foxx service's tests
// //////////////////////////////////////////////////////////////////////////////

function runTests (mount, options) {
  checkParameter(
    'runTests(<mount>, [<options>])',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]
  );

  var service = lookupService(mount);
  if (service.isDevelopment) {
    service.executeScript('setup');
  }
  ensureRouted(mount);
  var reporter = options ? options.reporter : null;
  return require('@arangodb/foxx/mocha').run(service, reporter);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Initializes the serviceCache and fills it initially for each db.
// //////////////////////////////////////////////////////////////////////////////

function initCache () {
  var dbname = arangodb.db._name();
  if (!serviceCache.hasOwnProperty(dbname)) {
    initializeFoxx();
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Internal scanFoxx function. Check scanFoxx.
// / Does not check parameters and throws errors.
// //////////////////////////////////////////////////////////////////////////////

function _scanFoxx (mount, options, activateDevelopment) {
  options = options || { };
  var dbname = arangodb.db._name();
  delete serviceCache[dbname][mount];
  var service = createService(mount, options, activateDevelopment);
  if (!options.__clusterDistribution) {
    db._executeTransaction({
      collections: {
        write: utils.getStorage().name()
      },
      action() {
        try {
          utils.getStorage().save(service.toJSON());
        } catch (err) {
          if (!options.replace || err.errorNum !== errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code) {
            throw err;
          }
          var old = utils.getStorage().firstExample({ mount: mount });
          if (old === null) {
            throw new ArangoError({
              errorNum: errors.ERROR_SERVICE_NOT_FOUND.code,
              errorMessage: dd`
                ${errors.ERROR_SERVICE_NOT_FOUND.message}
                Mount path: "${mount}".
              `
            });
          }
          var data = Object.assign({}, old);
          data.manifest = service.toJSON().manifest;
          utils.getStorage().replace(old, data);
        }
      }
    });
  }
  return service;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Scans the sources of the given mountpoint and publishes the routes
// /
// / TODO: Long Documentation!
// //////////////////////////////////////////////////////////////////////////////

function scanFoxx (mount, options) {
  checkParameter(
    'scanFoxx(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);
  initCache();
  var service = _scanFoxx(mount, options);
  reloadRouting();
  return service.simpleJSON();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Scans the sources of the given mountpoint and publishes the routes
// /
// / TODO: Long Documentation!
// //////////////////////////////////////////////////////////////////////////////

function rescanFoxx (mount) {
  checkParameter(
    'scanFoxx(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);

  var old = lookupService(mount);
  // var collection = utils.getStorage()
  initCache();
  _scanFoxx(
    mount,
    Object.assign({}, old.options, {replace: true}),
    old.isDevelopment
  );
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Build service in path
// //////////////////////////////////////////////////////////////////////////////

function _buildServiceInPath (serviceInfo, path, options) {
  try {
    if (serviceInfo === 'EMPTY') {
      // Make Empty service
      installServiceFromGenerator(path, options || {});
    } else if (/^GIT:/i.test(serviceInfo)) {
      installServiceFromRemote(buildGithubUrl(serviceInfo), path);
    } else if (/^https?:/i.test(serviceInfo)) {
      installServiceFromRemote(serviceInfo, path);
    } else if (utils.pathRegex.test(serviceInfo)) {
      installServiceFromLocal(serviceInfo, path);
    } else if (/^uploads[\/\\]tmp-/.test(serviceInfo)) {
      serviceInfo = joinPath(fs.getTempPath(), serviceInfo);
      installServiceFromLocal(serviceInfo, path);
    } else {
      if (!options || options.refresh !== false) {
        try {
          store.update();
        } catch (e) {
          console.warnLines(`Failed to update Foxx store: ${e.stack}`);
        }
      }
      const info = store.installationInfo(serviceInfo);
      installServiceFromRemote(info.url, path);
      patchManifestFile(path, info.manifest);
    }
    if (options.legacy) {
      patchManifestFile(path, {engines: {arangodb: '^2.8.0'}});
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief Internal service validation function
// / Does not check parameters and throws errors.
// //////////////////////////////////////////////////////////////////////////////

function _validateService (serviceInfo, mount) {
  var tempPath = fs.getTempFile('apps', false);
  try {
    _buildServiceInPath(serviceInfo, tempPath, {});
    var tmp = new FoxxService(fakeServiceConfig(tempPath, mount));
    if (!tmp.needsConfiguration()) {
      routeAndExportService(tmp, true);
    }
  } finally {
    fs.removeDirectoryRecursive(tempPath, true);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Internal install function. Check install.
// / Does not check parameters and throws errors.
// //////////////////////////////////////////////////////////////////////////////

function _install (serviceInfo, mount, options, runSetup) {
  var targetPath = computeServicePath(mount, true);
  var service;
  var collection = utils.getStorage();
  options = options || {};
  if (fs.exists(targetPath)) {
    throw new ArangoError({
      errorNum: errors.ERROR_SERVICE_MOUNTPOINT_CONFLICT.code,
      errorMessage: dd`
        ${errors.ERROR_SERVICE_MOUNTPOINT_CONFLICT.message}
        Mount path: "${mount}".
      `
    });
  }
  fs.makeDirectoryRecursive(targetPath);
  // Remove the empty APP folder.
  // Ohterwise move will fail.
  fs.removeDirectory(targetPath);

  initCache();
  _buildServiceInPath(serviceInfo, targetPath, options);
  try {
    service = _scanFoxx(mount, options);
    if (runSetup) {
      lookupService(mount).executeScript('setup');
    }
    if (!service.needsConfiguration()) {
      // Validate Routing & Exports
      routeAndExportService(service, true);
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
            if (definition !== null) {
              collection.remove(definition._key);
            }
          }
        });
      }
    } catch (err) {
      console.errorLines(err.stack);
    }
    throw e;
  }
  return service;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Installs a new foxx service on the given mount point.
// /
// / TODO: Long Documentation!
// //////////////////////////////////////////////////////////////////////////////

function install (serviceInfo, mount, options) {
  checkParameter(
    'install(<serviceInfo>, <mount>, [<options>])',
    [ [ 'Install information', 'string' ],
      [ 'Mount path', 'string' ] ],
    [ serviceInfo, mount ]);
  utils.validateMount(mount);
  let hasToBeDistributed = /^uploads[\/\\]tmp-/.test(serviceInfo);
  var service = _install(serviceInfo, mount, options, true);
  options = options || {};
  if (ArangoServerState.isCoordinator() && !options.__clusterDistribution) {
    let name = ArangoServerState.id();
    let coordinators = ArangoClusterInfo.getCoordinators().filter(function (c) {
      return c !== name;
    });
    if (hasToBeDistributed) {
      let result = uploadToPeerCoordinators(serviceInfo, coordinators);
      let mapping = result.mapping;
      let res = result.results;
      let intOpts = JSON.parse(JSON.stringify(options));
      intOpts.__clusterDistribution = true;
      let coordOptions = {
        coordTransactionID: ArangoClusterComm.getId()
      };
      let httpOptions = {};
      for (let i = 0; i < res.length; ++i) {
        let b = JSON.parse(res[i].body);
        /* jshint -W075:true */
        let intReq = {appInfo: b.filename, mount, options: intOpts};
        /* jshint -W075:false */
        ArangoClusterComm.asyncRequest('POST', 'server:' + mapping[res[i].clientTransactionID], db._name(),
          '/_admin/foxx/install', JSON.stringify(intReq), httpOptions, coordOptions);
      }
      cluster.wait(coordOptions, res.length);
    } else {
      /* jshint -W075:true */
      let req = {appInfo: serviceInfo, mount, options};
      /* jshint -W075:false */
      let httpOptions = {};
      let coordOptions = {
        coordTransactionID: ArangoClusterComm.getId()
      };
      req.options.__clusterDistribution = true;
      req = JSON.stringify(req);
      for (let i = 0; i < coordinators.length; ++i) {
        if (coordinators[i] !== ArangoServerState.id()) {
          ArangoClusterComm.asyncRequest('POST', 'server:' + coordinators[i], db._name(),
            '/_admin/foxx/install', req, httpOptions, coordOptions);
        }
      }
      cluster.wait(coordOptions, coordinators.length - 1);
    }
  }
  reloadRouting();
  return service.simpleJSON();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Internal install function. Check install.
// / Does not check parameters and throws errors.
// //////////////////////////////////////////////////////////////////////////////

function _uninstall (mount, options) {
  var dbname = arangodb.db._name();
  if (!serviceCache.hasOwnProperty(dbname)) {
    initializeFoxx(options);
  }
  var service;
  options = options || {};
  try {
    service = lookupService(mount);
  } catch (e) {
    if (!options.force) {
      throw e;
    }
  }
  var collection = utils.getStorage();
  var targetPath = computeServicePath(mount, true);
  delete serviceCache[dbname][mount];
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
      service.executeScript('teardown');
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
  if (options.force && service === undefined) {
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
  return service;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Uninstalls the foxx service on the given mount point.
// /
// / TODO: Long Documentation!
// //////////////////////////////////////////////////////////////////////////////

function uninstall (mount, options) {
  checkParameter(
    'uninstall(<mount>, [<options>])',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);
  utils.validateMount(mount);
  options = options || {};
  if (ArangoServerState.isCoordinator() && !options.__clusterDistribution) {
    let coordinators = ArangoClusterInfo.getCoordinators();
    /* jshint -W075:true */
    let req = {mount, options: JSON.parse(JSON.stringify(options))};
    /* jshint -W075:false */
    let httpOptions = {};
    let coordOptions = {
      coordTransactionID: ArangoClusterComm.getId()
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
    cluster.wait(coordOptions, coordinators.length - 1);
    require('internal').wait(1.0);
  }
  var service = _uninstall(mount, options);
  reloadRouting();
  return service.simpleJSON();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Replaces a foxx service on the given mount point by an other one.
// /
// / TODO: Long Documentation!
// //////////////////////////////////////////////////////////////////////////////

function replace (serviceInfo, mount, options) {
  checkParameter(
    'replace(<serviceInfo>, <mount>, [<options>])',
    [ [ 'Install information', 'string' ],
      [ 'Mount path', 'string' ] ],
    [ serviceInfo, mount ]);
  utils.validateMount(mount);
  _validateService(serviceInfo, mount);
  options = options || {};
  let hasToBeDistributed = /^uploads[\/\\]tmp-/.test(serviceInfo);
  if (ArangoServerState.isCoordinator() && !options.__clusterDistribution) {
    let name = ArangoServerState.id();
    let coordinators = ArangoClusterInfo.getCoordinators().filter(function (c) {
      return c !== name;
    });
    if (hasToBeDistributed) {
      let result = uploadToPeerCoordinators(serviceInfo, coordinators);
      let mapping = result.mapping;
      let res = result.results;
      let intOpts = JSON.parse(JSON.stringify(options));
      intOpts.__clusterDistribution = true;
      let coordOptions = {
        coordTransactionID: ArangoClusterComm.getId()
      };
      let httpOptions = {};
      for (let i = 0; i < res.length; ++i) {
        let b = JSON.parse(res[i].body);
        /* jshint -W075:true */
        let intReq = {appInfo: b.filename, mount, options: intOpts};
        /* jshint -W075:false */
        ArangoClusterComm.asyncRequest('POST', 'server:' + mapping[res[i].coordinatorTransactionID], db._name(),
          '/_admin/foxx/replace', JSON.stringify(intReq), httpOptions, coordOptions);
      }
      cluster.wait(coordOptions, res.length);
    } else {
      let intOpts = JSON.parse(JSON.stringify(options));
      /* jshint -W075:true */
      let req = {appInfo: serviceInfo, mount, options: intOpts};
      /* jshint -W075:false */
      let httpOptions = {};
      let coordOptions = {
        coordTransactionID: ArangoClusterComm.getId()
      };
      req.options.__clusterDistribution = true;
      req.options.force = true;
      req = JSON.stringify(req);
      for (let i = 0; i < coordinators.length; ++i) {
        ArangoClusterComm.asyncRequest('POST', 'server:' + coordinators[i], db._name(),
          '/_admin/foxx/replace', req, httpOptions, coordOptions);
      }
      cluster.wait(coordOptions, coordinators.length);
    }
  }
  _uninstall(mount, {teardown: true,
    __clusterDistribution: options.__clusterDistribution || false,
    force: !options.__clusterDistribution
  });
  var service = _install(serviceInfo, mount, options, true);
  reloadRouting();
  return service.simpleJSON();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Upgrade a foxx service on the given mount point by a new one.
// /
// / TODO: Long Documentation!
// //////////////////////////////////////////////////////////////////////////////

function upgrade (serviceInfo, mount, options) {
  checkParameter(
    'upgrade(<serviceInfo>, <mount>, [<options>])',
    [ [ 'Install information', 'string' ],
      [ 'Mount path', 'string' ] ],
    [ serviceInfo, mount ]);
  utils.validateMount(mount);
  _validateService(serviceInfo, mount);
  options = options || {};
  let hasToBeDistributed = /^uploads[\/\\]tmp-/.test(serviceInfo);
  if (ArangoServerState.isCoordinator() && !options.__clusterDistribution) {
    let name = ArangoServerState.id();
    let coordinators = ArangoClusterInfo.getCoordinators().filter(function (c) {
      return c !== name;
    });
    if (hasToBeDistributed) {
      let result = uploadToPeerCoordinators(serviceInfo, coordinators);
      let mapping = result.mapping;
      let res = result.results;
      let intOpts = JSON.parse(JSON.stringify(options));
      intOpts.__clusterDistribution = true;
      let coordOptions = {
        coordTransactionID: ArangoClusterComm.getId()
      };
      let httpOptions = {};
      for (let i = 0; i < res.length; ++i) {
        let b = JSON.parse(res[i].body);
        /* jshint -W075:true */
        let intReq = {appInfo: b.filename, mount, options: intOpts};
        /* jshint -W075:false */
        ArangoClusterComm.asyncRequest('POST', 'server:' + mapping[res[i].coordinatorTransactionID], db._name(),
          '/_admin/foxx/update', JSON.stringify(intReq), httpOptions, coordOptions);
      }
      cluster.wait(coordOptions, res.length);
    } else {
      let intOpts = JSON.parse(JSON.stringify(options));
      /* jshint -W075:true */
      let req = {appInfo: serviceInfo, mount, options: intOpts};
      /* jshint -W075:false */
      let httpOptions = {};
      let coordOptions = {
        coordTransactionID: ArangoClusterComm.getId()
      };
      req.options.__clusterDistribution = true;
      req.options.force = true;
      req = JSON.stringify(req);
      for (let i = 0; i < coordinators.length; ++i) {
        ArangoClusterComm.asyncRequest('POST', 'server:' + coordinators[i], db._name(),
          '/_admin/foxx/update', req, httpOptions, coordOptions);
      }
      cluster.wait(coordOptions, coordinators.length);
    }
  }
  var oldService = lookupService(mount);
  var oldConf = oldService.getConfiguration(true);
  options.configuration = options.configuration || {};
  Object.keys(oldConf).forEach(function (key) {
    if (!options.configuration.hasOwnProperty(key)) {
      options.configuration[key] = oldConf[key];
    }
  });
  var oldDeps = oldService.options.dependencies || {};
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
  var service = _install(serviceInfo, mount, options, true);
  reloadRouting();
  return service.simpleJSON();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief initializes the Foxx services
// //////////////////////////////////////////////////////////////////////////////

function initializeFoxx (options) {
  var dbname = arangodb.db._name();
  var mounts = syncWithFolder(options);
  refillCaches(dbname);
  checkMountedSystemService(dbname);
  for (const mount of mounts) {
    lookupService(mount).executeScript('setup');
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief compute all service routes
// //////////////////////////////////////////////////////////////////////////////

function mountPoints () {
  var dbname = arangodb.db._name();
  return refillCaches(dbname);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief toggles development mode of service and reloads routing
// //////////////////////////////////////////////////////////////////////////////

function _toggleDevelopment (mount, activate) {
  var service = lookupService(mount);
  service.development(activate);
  utils.updateService(mount, service.toJSON());
  if (!activate) {
    // Make sure setup changes from devmode are respected
    service.executeScript('setup');
  }
  reloadRouting();
  return service;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief activate development mode
// //////////////////////////////////////////////////////////////////////////////

function setDevelopment (mount) {
  checkParameter(
    'development(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);
  var service = _toggleDevelopment(mount, true);
  return service.simpleJSON();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief activate production mode
// //////////////////////////////////////////////////////////////////////////////

function setProduction (mount) {
  checkParameter(
    'production(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);
  var service = _toggleDevelopment(mount, false);
  return service.simpleJSON();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Configure the service at the mountpoint
// //////////////////////////////////////////////////////////////////////////////

function configure (mount, options) {
  checkParameter(
    'configure(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);
  utils.validateMount(mount, true);
  var service = lookupService(mount);
  var invalid = service.applyConfiguration(options.configuration || {});
  if (invalid.length > 0) {
    // TODO Error handling
    console.log(invalid);
  }
  utils.updateService(mount, service.toJSON());
  reloadRouting();
  return service.simpleJSON();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Set up dependencies of the service at the mountpoint
// //////////////////////////////////////////////////////////////////////////////

function updateDeps (mount, options) {
  checkParameter(
    'updateDeps(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);
  utils.validateMount(mount, true);
  var service = lookupService(mount);
  var invalid = service.applyDependencies(options.dependencies || {});
  if (invalid.length > 0) {
    // TODO Error handling
    console.log(invalid);
  }
  utils.updateService(mount, service.toJSON());
  reloadRouting();
  return service.simpleJSON();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Get the configuration for the service at the given mountpoint
// //////////////////////////////////////////////////////////////////////////////

function configuration (mount) {
  checkParameter(
    'configuration(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);
  utils.validateMount(mount, true);
  var service = lookupService(mount);
  return service.getConfiguration();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Get the dependencies for the service at the given mountpoint
// //////////////////////////////////////////////////////////////////////////////

function dependencies (mount) {
  checkParameter(
    'dependencies(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);
  utils.validateMount(mount, true);
  var service = lookupService(mount);
  return service.getDependencies();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Require the exports defined on the mount point
// //////////////////////////////////////////////////////////////////////////////

function requireService (mount) {
  checkParameter(
    'requireService(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);
  utils.validateMount(mount, true);
  var service = lookupService(mount);
  if (service.needsConfiguration()) {
    throw new ArangoError({
      errorNum: errors.ERROR_SERVICE_NEEDS_CONFIGURATION.code,
      errorMessage: errors.ERROR_SERVICE_NEEDS_CONFIGURATION.message
    });
  }
  return routeAndExportService(service, true).exports;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Syncs the services in ArangoDB with the services stored on disc
// //////////////////////////////////////////////////////////////////////////////

function syncWithFolder (options) {
  var dbname = arangodb.db._name();
  options = options || {};
  options.replace = true;
  serviceCache = serviceCache || {};
  serviceCache[dbname] = {};
  var folders = fs.listTree(FoxxService._appPath).filter(filterServiceRoots);
  //  var collection = utils.getStorage()
  return folders.map(function (folder) {
    var mount = transformPathToMount(folder);
    _scanFoxx(mount, options);
    return mount;
  });
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Exports
// //////////////////////////////////////////////////////////////////////////////

exports.syncWithFolder = syncWithFolder;
exports.install = install;
exports.readme = readme;
exports.runTests = runTests;
exports.runScript = runScript;
exports.setup = _.partial(runScript, 'setup');
exports.teardown = _.partial(runScript, 'teardown');
exports.uninstall = uninstall;
exports.replace = replace;
exports.upgrade = upgrade;
exports.development = setDevelopment;
exports.production = setProduction;
exports.configure = configure;
exports.updateDeps = updateDeps;
exports.configuration = configuration;
exports.dependencies = dependencies;
exports.requireService = requireService;
exports._resetCache = resetCache;

// //////////////////////////////////////////////////////////////////////////////
// / @brief Serverside only API
// //////////////////////////////////////////////////////////////////////////////

exports.scanFoxx = scanFoxx;
exports.mountPoints = mountPoints;
exports.routes = routes;
exports.ensureRouted = ensureRouted;
exports.rescanFoxx = rescanFoxx;
exports.lookupService = lookupService;

// //////////////////////////////////////////////////////////////////////////////
// / @brief Exports from foxx utils module.
// //////////////////////////////////////////////////////////////////////////////

exports.mountedService = utils.mountedService;
exports.list = utils.list;
exports.listJson = utils.listJson;
exports.listDevelopment = utils.listDevelopment;
exports.listDevelopmentJson = utils.listDevelopmentJson;

// //////////////////////////////////////////////////////////////////////////////
// / @brief Exports from foxx store module.
// //////////////////////////////////////////////////////////////////////////////

exports.available = store.available;
exports.availableJson = store.availableJson;
exports.getFishbowlStorage = store.getFishbowlStorage;
exports.search = store.search;
exports.searchJson = store.searchJson;
exports.update = store.update;
exports.info = store.info;

exports.initializeFoxx = initializeFoxx;

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

const fs = require('fs');
const path = require('path');
const dd = require('dedent');
const utils = require('@arangodb/foxx/manager-utils');
const store = require('@arangodb/foxx/store');
const FoxxService = require('@arangodb/foxx/service');
const ensureServiceExecuted = require('@arangodb/foxx/routing').routeService;
const arangodb = require('@arangodb');
const ArangoError = arangodb.ArangoError;
const errors = arangodb.errors;
const aql = arangodb.aql;
const db = arangodb.db;
const ArangoClusterControl = require('@arangodb/cluster');
const request = require('@arangodb/request');
const actions = require('@arangodb/actions');
const isZipBuffer = require('@arangodb/util').isZipBuffer;

const SYSTEM_SERVICE_MOUNTS = [
  '/_admin/aardvark', // Admin interface.
  '/_api/foxx', // Foxx management API.
  '/_api/gharial' // General_Graph API.
];

const GLOBAL_SERVICE_MAP = new Map();

// Cluster helpers

function getAllCoordinatorIds () {
  if (!ArangoClusterControl.isCluster()) {
    return [];
  }
  return global.ArangoClusterInfo.getCoordinators();
}

function getMyCoordinatorId () {
  if (!ArangoClusterControl.isCluster()) {
    return null;
  }
  return global.ArangoServerState.id();
}

function getPeerCoordinatorIds () {
  const myId = getMyCoordinatorId();
  return getAllCoordinatorIds().filter((id) => id !== myId);
}

function isFoxxmaster () {
  if (!ArangoClusterControl.isCluster()) {
    return true;
  }
  return global.ArangoServerState.isFoxxmaster();
}

function isClusterReadyForBusiness () {
  const coordIds = getPeerCoordinatorIds();
  return parallelClusterRequests(function * () {
    for (const coordId of coordIds) {
      yield [coordId, 'GET', '/_api/version'];
    }
  }()).every(response => response.statusCode === 200);
}

function parallelClusterRequests (requests) {
  let pending = 0;
  let options;
  const order = [];
  for (const [coordId, method, url, body, headers] of requests) {
    if (!options) {
      options = {coordTransactionID: global.ArangoClusterComm.getId()};
    }
    options.clientTransactionID = global.ArangoClusterInfo.uniqid();
    order.push(options.clientTransactionID);
    let actualBody;
    if (body) {
      if (typeof body === 'string') {
        actualBody = body;
      } else if (body instanceof Buffer) {
        if (body.length) {
          actualBody = body;
        }
      } else {
        actualBody = JSON.stringify(body);
      }
    }
    global.ArangoClusterComm.asyncRequest(
      method,
      `server:${coordId}`,
      db._name(),
      url,
      actualBody || undefined,
      headers || {},
      options
    );
    pending++;
  }
  if (!pending) {
    return [];
  }
  delete options.clientTransactionID;
  const results = ArangoClusterControl.wait(options, pending, true);
  return results.sort(
    (a, b) => order.indexOf(a.clientTransactionID) - order.indexOf(b.clientTransactionID)
  );
}

// Startup and self-heal

function selfHealAll (skipReloadRouting) {
  const db = require('internal').db;
  const dbName = db._name();
  let modified;
  try {
    db._useDatabase('_system');
    const databases = db._databases();
    for (const name of databases) {
      try {
        db._useDatabase(name);
        modified = selfHeal() || modified;
      } catch (e) {
        console.debugStack(e);
      }
    }
  } finally {
    db._useDatabase(dbName);
    if (modified && !skipReloadRouting) {
      reloadRouting();
    }
  }
}

function triggerSelfHeal () {
  const modified = selfHeal();
  if (modified) {
    reloadRouting();
  }
}

function selfHeal () {
  const dirname = FoxxService.rootBundlePath();
  if (!fs.exists(dirname)) {
    fs.makeDirectoryRecursive(dirname);
  }

  const serviceCollection = utils.getStorage();
  const bundleCollection = utils.getBundleStorage();
  const serviceDefinitions = db._query(aql`
    FOR doc IN ${serviceCollection}
    FILTER LEFT(doc.mount, 2) != "/_"
    LET bundleExists = DOCUMENT(${bundleCollection}, doc.checksum) != null
    RETURN [doc.mount, doc.checksum, doc._rev, bundleExists]
  `).toArray();

  let modified = false;
  const knownBundlePaths = new Array(serviceDefinitions.length);
  const knownServicePaths = new Array(serviceDefinitions.length);
  const localServiceMap = GLOBAL_SERVICE_MAP.get(db._name());
  for (const [mount, checksum, rev, bundleExists] of serviceDefinitions) {
    const bundlePath = FoxxService.bundlePath(mount);
    const basePath = FoxxService.basePath(mount);
    knownBundlePaths.push(bundlePath);
    knownServicePaths.push(basePath);

    if (localServiceMap) {
      if (!localServiceMap.has(mount) || localServiceMap.get(mount)._rev !== rev) {
        modified = true;
      }
    }

    const hasBundle = fs.exists(bundlePath);
    const hasFolder = fs.exists(basePath);
    if (!hasBundle && hasFolder) {
      createServiceBundle(mount);
    } else if (hasBundle && !hasFolder) {
      extractServiceBundle(bundlePath, basePath);
      modified = true;
    }
    const isInstalled = hasBundle || hasFolder;
    const localChecksum = isInstalled ? safeChecksum(mount) : null;

    if (!checksum) {
      // pre-3.2 service, can't self-heal this
      continue;
    }

    if (checksum === localChecksum) {
      if (!bundleExists) {
        try {
          bundleCollection._binaryInsert({_key: checksum}, bundlePath);
        } catch (e) {
          console.warnStack(e, `Failed to store missing service bundle for service at "${mount}"`);
        }
      }
    } else if (bundleExists) {
      try {
        if (fs.exists(bundlePath)) {
          fs.remove(bundlePath);
        }
        bundleCollection._binaryDocument(checksum, bundlePath);
        extractServiceBundle(bundlePath, basePath);
        modified = true;
      } catch (e) {
        console.errorStack(e, `Failed to load service bundle for service at "${mount}"`);
      }
    }
  }

  const rootPath = FoxxService.rootPath();
  for (const relPath of fs.listTree(rootPath)) {
    if (!relPath) {
      continue;
    }
    const basename = path.basename(relPath);
    if (basename.toUpperCase() !== 'APP') {
      continue;
    }
    const basePath = path.resolve(rootPath, relPath);
    if (!knownServicePaths.includes(basePath)) {
      modified = true;
      try {
        fs.removeDirectoryRecursive(basePath, true);
        console.debug(`Deleted orphaned service folder ${basePath}`);
      } catch (e) {
        console.warnStack(e, `Failed to delete orphaned service folder ${basePath}`);
      }
    }
  }

  const bundlesPath = FoxxService.rootBundlePath();
  for (const relPath of fs.listTree(bundlesPath)) {
    if (!relPath) {
      continue;
    }
    const bundlePath = path.resolve(bundlesPath, relPath);
    if (!knownBundlePaths.includes(bundlePath)) {
      try {
        fs.remove(bundlePath);
        console.debug(`Deleted orphaned service bundle ${bundlePath}`);
      } catch (e) {
        console.warnStack(e, `Failed to delete orphaned service bundle ${bundlePath}`);
      }
    }
  }

  return modified;
}

function startup () {
  if (isFoxxmaster()) {
    const db = require('internal').db;
    const dbName = db._name();
    try {
      db._useDatabase('_system');
      const databases = db._databases();
      for (const name of databases) {
        try {
          db._useDatabase(name);
          upsertSystemServices();
        } catch (e) {
          console.warnStack(e);
        }
      }
    } finally {
      db._useDatabase(dbName);
    }
  }
  if (global.ArangoServerState.role() === 'SINGLE') {
    commitLocalState(true);
  }
  selfHealAll(true);
}

function upsertSystemServices () {
  const serviceDefinitions = new Map();
  for (const mount of SYSTEM_SERVICE_MOUNTS) {
    const serviceDefinition = utils.getServiceDefinition(mount) || {mount};
    const service = FoxxService.create(serviceDefinition);
    serviceDefinitions.set(mount, service.toJSON());
  }
  db._query(aql`
    FOR item IN ${Array.from(serviceDefinitions)}
    UPSERT {mount: item[0]}
    INSERT item[1]
    REPLACE item[1]
    IN ${utils.getStorage()}
  `);
}

function commitLocalState (replace) {
  let modified = false;
  const rootPath = FoxxService.rootPath();
  const collection = utils.getStorage();
  const bundleCollection = utils.getBundleStorage();
  for (const relPath of fs.listTree(rootPath)) {
    if (!relPath) {
      continue;
    }
    const basename = path.basename(relPath);
    if (basename.toUpperCase() !== 'APP') {
      continue;
    }
    const mount = '/' + path.dirname(relPath).split(path.sep).join('/');
    const basePath = FoxxService.basePath(mount);
    if (!fs.list(basePath).length) {
      continue;
    }
    const bundlePath = FoxxService.bundlePath(mount);
    if (!fs.exists(bundlePath)) {
      createServiceBundle(mount, bundlePath);
    }
    const serviceDefinition = db._query(aql`
      FOR service IN ${collection}
      FILTER service.mount == ${mount}
      RETURN service
    `).next();
    if (!serviceDefinition) {
      try {
        const service = FoxxService.create({mount});
        service.updateChecksum();
        if (!bundleCollection.exists(service.checksum)) {
          bundleCollection._binaryInsert({_key: service.checksum}, bundlePath);
        }
        collection.save(service.toJSON());
        modified = true;
      } catch (e) {
        console.errorStack(e);
      }
    } else {
      const checksum = safeChecksum(mount);
      if (!serviceDefinition.checksum || (replace && serviceDefinition.checksum !== checksum)) {
        if (!bundleCollection.exists(checksum)) {
          bundleCollection._binaryInsert({_key: checksum}, bundlePath);
        }
        collection.update(serviceDefinition._key, {checksum});
        modified = true;
      } else if (serviceDefinition.checksum === checksum) {
        if (!bundleCollection.exists(checksum)) {
          bundleCollection._binaryInsert({_key: checksum}, bundlePath);
          modified = true;
        }
      }
    }
  }
  if (modified) {
    propagateSelfHeal();
  }
}

// Change propagation

function reloadRouting () {
  require('internal').executeGlobalContextFunction('reloadRouting');
  actions.reloadRouting();
}

function propagateSelfHeal () {
  try {
    parallelClusterRequests(function * () {
      for (const coordId of getPeerCoordinatorIds()) {
        yield [coordId, 'POST', '/_api/foxx/_local/heal'];
      }
    }());
  } catch (e) {
    console.errorStack(e, 'Failure during propagate self heal');
  }
  reloadRouting();
}

// GLOBAL_SERVICE_MAP manipulation

function initLocalServiceMap () {
  const localServiceMap = new Map();
  for (const mount of SYSTEM_SERVICE_MOUNTS) {
    const serviceDefinition = utils.getServiceDefinition(mount) || {mount};
    const service = FoxxService.create(serviceDefinition);
    localServiceMap.set(service.mount, service);
  }
  for (const serviceDefinition of utils.getStorage().all()) {
    try {
      const service = loadInstalledService(serviceDefinition);
      localServiceMap.set(serviceDefinition.mount, service);
    } catch (e) {
      localServiceMap.set(serviceDefinition.mount, {error: e});
    }
  }
  GLOBAL_SERVICE_MAP.set(db._name(), localServiceMap);
}

function ensureFoxxInitialized () {
  if (!GLOBAL_SERVICE_MAP.has(db._name())) {
    initLocalServiceMap();
  }
}

function ensureServiceLoaded (mount) {
  const service = getServiceInstance(mount);
  return ensureServiceExecuted(service, false);
}

function getServiceInstance (mount) {
  ensureFoxxInitialized();
  const localServiceMap = GLOBAL_SERVICE_MAP.get(db._name());
  let service;
  if (localServiceMap.has(mount)) {
    service = localServiceMap.get(mount);
  } else {
    service = reloadInstalledService(mount);
  }
  if (!service) {
    throw new ArangoError({
      errorNum: errors.ERROR_SERVICE_NOT_FOUND.code,
      errorMessage: dd`
        ${errors.ERROR_SERVICE_NOT_FOUND.message}
        Mount path: "${mount}".
      `
    });
  }
  if (service.error) {
    throw service.error;
  }
  return service;
}

function reloadInstalledService (mount, runSetup) {
  const serviceDefinition = utils.getServiceDefinition(mount);
  if (!serviceDefinition) {
    throw new ArangoError({
      errorNum: errors.ERROR_SERVICE_NOT_FOUND.code,
      errorMessage: dd`
        ${errors.ERROR_SERVICE_NOT_FOUND.message}
        Mount path: "${mount}".
      `
    });
  }
  const service = loadInstalledService(serviceDefinition);
  if (runSetup) {
    service.executeScript('setup');
  }
  GLOBAL_SERVICE_MAP.get(db._name()).set(service.mount, service);
  return service;
}

function loadInstalledService (serviceDefinition) {
  const mount = serviceDefinition.mount;
  if (!mount.startsWith('/_')) {
    if (
      !fs.exists(FoxxService.bundlePath(mount)) ||
      !fs.exists(FoxxService.basePath(mount))
    ) {
      throw new ArangoError({
        errorNum: errors.ERROR_SERVICE_FILES_MISSING.code,
        errorMessage: dd`
          ${errors.ERROR_SERVICE_FILES_MISSING.message}
          Mount: ${mount}
        `
      });
    }
    const checksum = serviceDefinition.checksum;
    if (checksum && checksum !== safeChecksum(mount)) {
      throw new ArangoError({
        errorNum: errors.ERROR_SERVICE_FILES_OUTDATED.code,
        errorMessage: dd`
          ${errors.ERROR_SERVICE_FILES_OUTDATED.message}
          Mount: ${mount}
        `
      });
    }
  }
  return FoxxService.create(serviceDefinition);
}

// Misc?

function patchManifestFile (servicePath, patchData) {
  const filename = path.join(servicePath, 'manifest.json');
  let manifest;
  try {
    const rawManifest = fs.readFileSync(filename, 'utf-8');
    manifest = JSON.parse(rawManifest);
  } catch (e) {
    throw Object.assign(
      new ArangoError({
        errorNum: errors.ERROR_MALFORMED_MANIFEST_FILE.code,
        errorMessage: dd`
          ${errors.ERROR_MALFORMED_MANIFEST_FILE.message}
          File: ${filename}
        `
      }), {cause: e}
    );
  }
  Object.assign(manifest, patchData);
  fs.writeFileSync(filename, JSON.stringify(manifest, null, 2));
}

function _prepareService (serviceInfo, options = {}) {
  const tempServicePath = utils.joinLastPath(fs.getTempFile('services', false));
  const tempBundlePath = utils.joinLastPath(fs.getTempFile('bundles', false));
  try {
    if (isZipBuffer(serviceInfo)) {
      // Buffer (zip)
      const tempFile = fs.getTempFile('bundles', false);
      fs.writeFileSync(tempFile, serviceInfo);
      extractServiceBundle(tempFile, tempServicePath);
      fs.move(tempFile, tempBundlePath);
    } else if (serviceInfo instanceof Buffer) {
      // Buffer (js)
      const manifest = JSON.stringify({main: 'index.js'}, null, 4);
      fs.makeDirectoryRecursive(tempServicePath);
      fs.writeFileSync(path.join(tempServicePath, 'index.js'), serviceInfo);
      fs.writeFileSync(path.join(tempServicePath, 'manifest.json'), manifest);
      utils.zipDirectory(tempServicePath, tempBundlePath);
    } else if (/^https?:/i.test(serviceInfo)) {
      // Remote path
      const tempFile = downloadServiceBundleFromRemote(serviceInfo);
      extractServiceBundle(tempFile, tempServicePath);
      fs.move(tempFile, tempBundlePath);
    } else if (fs.exists(serviceInfo)) {
      // Local path
      if (fs.isDirectory(serviceInfo)) {
        utils.zipDirectory(serviceInfo, tempBundlePath);
        extractServiceBundle(tempBundlePath, tempServicePath);
      } else {
        extractServiceBundle(serviceInfo, tempServicePath);
        fs.copyFile(serviceInfo, tempBundlePath);
      }
    } else {
      // Foxx Store
      if (options.refresh) {
        try {
          store.update();
        } catch (e) {
          console.warnStack(e);
        }
      }
      const info = store.installationInfo(serviceInfo);
      if (!info) {
        throw new ArangoError({
          errorNum: errors.ERROR_SERVICE_SOURCE_NOT_FOUND.code,
          errorMessage: dd`
            ${errors.ERROR_SERVICE_SOURCE_NOT_FOUND.message}
            Location: ${serviceInfo}
          `
        });
      }
      const storeBundle = downloadServiceBundleFromRemote(info.url);
      try {
        extractServiceBundle(storeBundle, tempServicePath);
      } finally {
        try {
          fs.remove(storeBundle);
        } catch (e) {
          console.warnStack(e, `Cannot remove temporary file "${storeBundle}"`);
        }
      }
      patchManifestFile(tempServicePath, info.manifest);
      utils.zipDirectory(tempServicePath, tempBundlePath);
    }
    if (options.legacy) {
      patchManifestFile(tempServicePath, {engines: {arangodb: '^2.8.0'}});
      if (fs.exists(tempBundlePath)) {
        fs.remove(tempBundlePath);
      }
      utils.zipDirectory(tempServicePath, tempBundlePath);
    }
    return {
      tempServicePath,
      tempBundlePath
    };
  } catch (e) {
    try {
      fs.removeDirectoryRecursive(tempServicePath, true);
    } catch (e2) {}
    throw e;
  }
}

function _buildServiceInPath (mount, tempServicePath, tempBundlePath) {
  const servicePath = FoxxService.basePath(mount);
  if (fs.exists(servicePath)) {
    fs.removeDirectoryRecursive(servicePath, true);
  }
  fs.makeDirectoryRecursive(path.dirname(servicePath));
  fs.move(tempServicePath, servicePath);
  const bundlePath = FoxxService.bundlePath(mount);
  if (fs.exists(bundlePath)) {
    fs.remove(bundlePath);
  }
  fs.makeDirectoryRecursive(path.dirname(bundlePath));
  fs.move(tempBundlePath, bundlePath);
}

function _deleteServiceFromPath (mount, options) {
  const servicePath = FoxxService.basePath(mount);
  if (fs.exists(servicePath)) {
    try {
      fs.removeDirectoryRecursive(servicePath, true);
    } catch (e) {
      if (!options.force) {
        throw e;
      }
      console.warnStack(e);
    }
  }
  const bundlePath = FoxxService.bundlePath(mount);
  if (fs.exists(bundlePath)) {
    try {
      fs.remove(bundlePath);
    } catch (e) {
      if (!options.force) {
        throw e;
      }
      console.warnStack(e);
    }
  }
}

function _install (mount, options = {}) {
  const collection = utils.getStorage();
  let service;
  try {
    service = FoxxService.create({
      mount,
      options,
      noisy: true
    });
    if (options.setup !== false) {
      service.executeScript('setup');
    }
  } catch (e) {
    if (!options.force) {
      _deleteServiceFromPath(mount, options);
      throw e;
    } else {
      console.warnStack(e);
    }
  }
  service.updateChecksum();
  const bundleCollection = utils.getBundleStorage();
  if (!bundleCollection.exists(service.checksum)) {
    bundleCollection._binaryInsert({_key: service.checksum}, service.bundlePath);
  }
  const serviceDefinition = service.toJSON();
  const meta = db._query(aql`
    UPSERT {mount: ${mount}}
    INSERT ${serviceDefinition}
    REPLACE ${serviceDefinition}
    IN ${collection}
    RETURN NEW
  `).next();
  service._rev = meta._rev;
  GLOBAL_SERVICE_MAP.get(db._name()).set(mount, service);
  try {
    ensureServiceExecuted(service, true);
  } catch (e) {
    if (!options.force) {
      console.errorStack(e);
    } else {
      console.warnStack(e);
    }
  }
  return service;
}

function _uninstall (mount, options = {}) {
  let service;
  try {
    service = getServiceInstance(mount);
    if (service.error) {
      const error = service.error;
      service = null;
      throw error;
    }
  } catch (e) {
    if (!options.force) {
      throw e;
    }
  }
  if (service && options.teardown !== false) {
    try {
      service.executeScript('teardown');
    } catch (e) {
      if (!options.force) {
        throw e;
      }
      console.warnStack(e);
    }
  }
  const collection = utils.getStorage();
  const serviceDefinition = db._query(aql`
    FOR service IN ${collection}
    FILTER service.mount == ${mount}
    REMOVE service IN ${collection}
    RETURN OLD
  `).next();
  if (serviceDefinition) {
    const checksumRefs = db._query(aql`
      FOR service IN ${collection}
      FILTER service.checksum == ${serviceDefinition.checksum}
      RETURN 1
    `).toArray();
    const bundleCollection = utils.getBundleStorage();
    if (!checksumRefs.length && bundleCollection.exists(serviceDefinition.checksum)) {
      bundleCollection.remove(serviceDefinition.checksum);
    }
  }
  GLOBAL_SERVICE_MAP.get(db._name()).delete(mount);
  _deleteServiceFromPath(mount, options);
  return service;
}

// Service bundle manipulation

function createServiceBundle (mount, bundlePath = FoxxService.bundlePath(mount)) {
  const servicePath = FoxxService.basePath(mount);
  if (fs.exists(bundlePath)) {
    fs.remove(bundlePath);
  }
  fs.makeDirectoryRecursive(path.dirname(bundlePath));
  utils.zipDirectory(servicePath, bundlePath);
}

function downloadServiceBundleFromRemote (url) {
  try {
    const res = request.get(url, {encoding: null});
    if (res.json && res.json.errorNum) {
      throw new ArangoError(res.json);
    }
    res.throw();
    const tempFile = fs.getTempFile('bundles', false);
    fs.writeFileSync(tempFile, res.body);
    return tempFile;
  } catch (e) {
    throw Object.assign(
      new ArangoError({
        errorNum: errors.ERROR_SERVICE_SOURCE_ERROR.code,
        errorMessage: dd`
          ${errors.ERROR_SERVICE_SOURCE_ERROR.message}
          URL: ${url}
        `
      }),
      {cause: e}
    );
  }
}

function extractServiceBundle (archive, targetPath) {
  const tempFolder = utils.joinLastPath(fs.getTempFile('services', false));
  fs.makeDirectory(tempFolder);
  fs.unzipFile(archive, tempFolder, false, true);

  let manifestPath;
  // find the manifest with the shortest path
  const filenames = fs.listTree(tempFolder).sort((a, b) => a.length - b.length);
  for (const filename of filenames) {
    if (filename === 'manifest.json' || filename.endsWith('/manifest.json') || filename.endsWith('\\manifest.json')) {
      manifestPath = filename;
      break;
    }
  }

  if (!manifestPath) {
    throw new ArangoError({
      errorNum: errors.ERROR_SERVICE_MANIFEST_NOT_FOUND.code,
      errorMessage: dd`
        ${errors.ERROR_SERVICE_MANIFEST_NOT_FOUND.message}
        Source: ${tempFolder}
      `
    });
  }

  let basePath = path.dirname(path.resolve(tempFolder, manifestPath));
  if (fs.exists(targetPath)) {
    fs.removeDirectoryRecursive(targetPath, true);
  }
  fs.move(basePath, targetPath);

  if (manifestPath.endsWith('/manifest.json') || manifestPath.endsWith('\\manifest.json')) {
    // service basePath is a subfolder of tempFolder
    // so tempFolder still exists and needs to be removed
    fs.removeDirectoryRecursive(tempFolder, true);
  }
}

// Exported functions for manipulating services

function install (serviceInfo, mount, options = {}) {
  utils.validateMount(mount);
  ensureFoxxInitialized();
  if (utils.getServiceDefinition(mount)) {
    throw new ArangoError({
      errorNum: errors.ERROR_SERVICE_MOUNTPOINT_CONFLICT.code,
      errorMessage: dd`
        ${errors.ERROR_SERVICE_MOUNTPOINT_CONFLICT.message}
        Mount path: "${mount}".
      `
    });
  }
  const tempPaths = _prepareService(serviceInfo, options);
  _buildServiceInPath(mount, tempPaths.tempServicePath, tempPaths.tempBundlePath);
  const service = _install(mount, options);
  propagateSelfHeal();
  return service;
}

function uninstall (mount, options = {}) {
  ensureFoxxInitialized();
  const service = _uninstall(mount, options);
  propagateSelfHeal();
  return service;
}

function replace (serviceInfo, mount, options = {}) {
  utils.validateMount(mount);
  ensureFoxxInitialized();
  const tempPaths = _prepareService(serviceInfo, options);
  FoxxService.validatedManifest({
    mount,
    basePath: tempPaths.tempServicePath,
    noisy: true
  });
  _uninstall(mount, Object.assign({teardown: true}, options, {force: true}));
  _buildServiceInPath(mount, tempPaths.tempServicePath, tempPaths.tempBundlePath);
  const service = _install(mount, Object.assign({}, options, {force: true}));
  propagateSelfHeal();
  return service;
}

function upgrade (serviceInfo, mount, options = {}) {
  ensureFoxxInitialized();
  const serviceOptions = utils.getServiceDefinition(mount).options;
  Object.assign(serviceOptions.configuration, options.configuration);
  Object.assign(serviceOptions.dependencies, options.dependencies);
  serviceOptions.development = options.development;
  const tempPaths = _prepareService(serviceInfo, options);
  FoxxService.validatedManifest({
    mount,
    basePath: tempPaths.tempServicePath,
    noisy: true
  });
  _uninstall(mount, Object.assign({teardown: false}, options, {force: true}));
  _buildServiceInPath(mount, tempPaths.tempServicePath, tempPaths.tempBundlePath);
  const service = _install(mount, Object.assign({}, options, serviceOptions, {force: true}));
  propagateSelfHeal();
  return service;
}

function runScript (scriptName, mount, options) {
  let service = getServiceInstance(mount);
  if (service.error) {
    throw service.error;
  }
  if (service.isDevelopment) {
    const runSetup = scriptName !== 'setup';
    service = reloadInstalledService(mount, runSetup);
  }
  ensureServiceLoaded(mount);
  const result = service.executeScript(scriptName, options);
  return result === undefined ? null : result;
}

function runTests (mount, options = {}) {
  let service = getServiceInstance(mount);
  if (service.error) {
    throw service.error;
  }
  if (service.isDevelopment) {
    service = reloadInstalledService(mount, true);
  }
  ensureServiceLoaded(mount);
  return require('@arangodb/foxx/mocha').run(service, options.reporter);
}

function enableDevelopmentMode (mount) {
  const service = getServiceInstance(mount);
  if (service.error) {
    throw service.error;
  }
  service.development(true);
  utils.updateService(mount, service.toJSON());
  propagateSelfHeal();
  return service;
}

function disableDevelopmentMode (mount) {
  const service = getServiceInstance(mount);
  if (service.error) {
    throw service.error;
  }
  service.development(false);
  createServiceBundle(mount);
  service.updateChecksum();
  const bundleCollection = utils.getBundleStorage();
  if (!bundleCollection.exists(service.checksum)) {
    bundleCollection._binaryInsert({_key: service.checksum}, service.bundlePath);
  }
  utils.updateService(mount, service.toJSON());
  // Make sure setup changes from devmode are respected
  service.executeScript('setup');
  propagateSelfHeal();
  return service;
}

function setConfiguration (mount, options = {}) {
  const service = getServiceInstance(mount);
  if (service.error) {
    throw service.error;
  }
  const warnings = service.applyConfiguration(options.configuration, options.replace);
  utils.updateService(mount, service.toJSON());
  propagateSelfHeal();
  return warnings;
}

function setDependencies (mount, options = {}) {
  const service = getServiceInstance(mount);
  if (service.error) {
    throw service.error;
  }
  const warnings = service.applyDependencies(options.dependencies, options.replace);
  utils.updateService(mount, service.toJSON());
  propagateSelfHeal();
  return warnings;
}

// Misc exported functions

function requireService (mount) {
  mount = '/' + mount.replace(/^\/+|\/+$/g, '');
  const service = getServiceInstance(mount);
  if (service.error) {
    // TODO Bad requires should probably always blow up
    return {};
  }
  return ensureServiceExecuted(service, true).exports;
}

function getMountPoints () {
  ensureFoxxInitialized();
  return Array.from(GLOBAL_SERVICE_MAP.get(db._name()).keys());
}

function installedServices () {
  ensureFoxxInitialized();
  return Array.from(GLOBAL_SERVICE_MAP.get(db._name()).values()).filter(service => !service.error);
}

function safeChecksum (mount) {
  try {
    return FoxxService.checksum(mount);
  } catch (e) {
    return null;
  }
}

// Exports

exports.install = install;
exports.uninstall = uninstall;
exports.replace = replace;
exports.upgrade = upgrade;
exports.runTests = runTests;
exports.runScript = runScript;
exports.development = enableDevelopmentMode;
exports.production = disableDevelopmentMode;
exports.setConfiguration = setConfiguration;
exports.setDependencies = setDependencies;
exports.requireService = requireService;
exports.lookupService = getServiceInstance;
exports.installedServices = installedServices;

// -------------------------------------------------
// Exported internals
// -------------------------------------------------

exports.isFoxxmaster = isFoxxmaster;
exports._reloadRouting = reloadRouting;
exports.reloadInstalledService = reloadInstalledService;
exports.ensureRouted = ensureServiceLoaded;
exports.initializeFoxx = initLocalServiceMap;
exports.ensureFoxxInitialized = ensureFoxxInitialized;
exports._startup = startup;
exports.heal = triggerSelfHeal;
exports.healAll = selfHealAll;
exports.commitLocalState = commitLocalState;
exports._createServiceBundle = createServiceBundle;
exports._resetCache = () => GLOBAL_SERVICE_MAP.clear();
exports._mountPoints = getMountPoints;
exports._isClusterReady = isClusterReadyForBusiness;

// -------------------------------------------------
// Exports from foxx utils module
// -------------------------------------------------

exports.getServiceDefinition = utils.getServiceDefinition;
exports.list = utils.list;
exports.listDevelopment = utils.listDevelopment;
exports.listDevelopmentJson = utils.listDevelopmentJson;

// -------------------------------------------------
// Exports from foxx store module
// -------------------------------------------------

exports.available = store.available;
exports.availableJson = store.availableJson;
exports.getFishbowlStorage = store.getFishbowlStorage;
exports.search = store.search;
exports.searchJson = store.searchJson;
exports.update = store.update;
exports.info = store.info;

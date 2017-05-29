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
const querystringify = require('querystring').encode;
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
const shuffle = require('lodash/shuffle');
const zip = require('lodash/zip');
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

function getFoxmasterCoordinatorId () {
  if (!ArangoClusterControl.isCluster()) {
    return null;
  }
  return global.ArangoServerState.getFoxxmaster();
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

function proxyToFoxxmaster (req, res) {
  const coordId = getFoxmasterCoordinatorId();
  const response = parallelClusterRequests([[
    coordId,
    req.method,
    req._url.pathname + (req._url.search || ''),
    req.rawBody,
    req.headers
  ]])[0];
  res.statusCode = response.statusCode;
  res.headers = response.headers;
  res.body = response.rawBody;
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

function isFoxxmasterReady () {
  if (!ArangoClusterControl.isCluster()) {
    return true;
  }
  const coordId = getFoxmasterCoordinatorId();
  const response = parallelClusterRequests([[
    coordId,
    'GET',
    '/_api/foxx/_local/status'
  ]])[0];
  if (response.statusCode >= 400) {
    return false;
  }
  return JSON.parse(response.body).ready;
}

function getChecksumsFromPeers (mounts) {
  const coordinatorIds = getPeerCoordinatorIds();
  const responses = parallelClusterRequests(function * () {
    for (const coordId of coordinatorIds) {
      yield [
        coordId,
        'GET',
        `/_api/foxx/_local/checksums?${querystringify({mount: mounts})}`
      ];
    }
  }());
  const peerChecksums = new Map();
  for (const [coordId, response] of zip(coordinatorIds, responses)) {
    const body = JSON.parse(response.body);
    const coordChecksums = new Map();
    for (const mount of mounts) {
      coordChecksums.set(mount, body[mount] || null);
    }
    peerChecksums.set(coordId, coordChecksums);
  }
  return peerChecksums;
}

// Startup and self-heal

function startup () {
  const db = require('internal').db;
  const dbName = db._name();
  try {
    db._useDatabase('_system');
    const writeToDatabase = isFoxxmaster();
    const databases = db._databases();
    for (const name of databases) {
      try {
        db._useDatabase(name);
        rebuildAllServiceBundles(writeToDatabase);
        if (writeToDatabase) {
          upsertSystemServices();
        }
      } catch (e) {
        console.warnStack(e);
      }
    }
  } finally {
    db._useDatabase(dbName);
  }
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

function rebuildAllServiceBundles (fixMissingChecksums) {
  const servicesMissingChecksums = [];
  const collection = utils.getStorage();
  for (const serviceDefinition of collection.all()) {
    const mount = serviceDefinition.mount;
    if (mount.startsWith('/_')) {
      continue;
    }
    const bundlePath = FoxxService.bundlePath(mount);
    const basePath = FoxxService.basePath(mount);

    const hasBundle = fs.exists(bundlePath);
    const hasFolder = fs.exists(basePath);
    if (!hasBundle && hasFolder) {
      createServiceBundle(mount);
    } else if (hasBundle && !hasFolder) {
      extractServiceBundle(bundlePath, basePath);
    } else if (!hasBundle && !hasFolder) {
      continue;
    }
    if (fixMissingChecksums && !serviceDefinition.checksum) {
      servicesMissingChecksums.push({
        checksum: FoxxService.checksum(mount),
        _key: serviceDefinition._key
      });
    }
  }
  if (!servicesMissingChecksums.length) {
    return;
  }
  db._query(aql`
    FOR service IN ${servicesMissingChecksums}
    UPDATE service._key
    WITH {checksum: service.checksum}
    IN ${collection}
  `);
}

function selfHeal () {
  const db = require('internal').db;
  const dbName = db._name();
  try {
    db._useDatabase('_system');
    const databases = db._databases();
    const weAreFoxxmaster = isFoxxmaster();
    const foxxmasterIsReady = weAreFoxxmaster || isFoxxmasterReady();
    for (const name of databases) {
      try {
        db._useDatabase(name);
        if (weAreFoxxmaster) {
          healMyselfAndCoords();
        } else if (foxxmasterIsReady) {
          healMyself();
        }
      } catch (e) {
        console.warnStack(e);
      }
    }
    return foxxmasterIsReady;
  } finally {
    db._useDatabase(dbName);
  }
}

function healMyself () {
  const servicesINeedToFix = new Map();

  const collection = utils.getStorage();
  for (const serviceDefinition of collection.all()) {
    const mount = serviceDefinition.mount;
    const checksum = serviceDefinition.checksum;
    if (mount.startsWith('/_')) {
      continue;
    }
    if (!checksum || checksum !== safeChecksum(mount)) {
      servicesINeedToFix.set(mount, checksum);
    }
  }

  const coordinatorIds = getPeerCoordinatorIds();
  let clusterIsInconsistent = false;
  for (const [mount, checksum] of servicesINeedToFix) {
    const coordIdsToTry = shuffle(coordinatorIds);
    let found = false;
    for (const coordId of coordIdsToTry) {
      const bundle = downloadServiceBundleFromCoordinator(coordId, mount, checksum);
      if (bundle) {
        replaceLocalServiceFromTempBundle(mount, bundle);
        found = true;
        break;
      }
    }
    if (!found) {
      clusterIsInconsistent = true;
      break;
    }
  }

  if (clusterIsInconsistent) {
    const coordId = getFoxmasterCoordinatorId();
    parallelClusterRequests([[
      coordId,
      'POST',
      '/_api/foxx/_local/heal'
    ]]);
  }
}

function healMyselfAndCoords () {
  const checksumsINeedToFixLocally = [];
  const actualChecksums = new Map();
  const coordsKnownToBeGoodSources = new Map();
  const coordsKnownToBeBadSources = new Map();
  const allKnownMounts = [];

  const collection = utils.getStorage();
  for (const serviceDefinition of collection.all()) {
    const mount = serviceDefinition.mount;
    const checksum = serviceDefinition.checksum;
    if (mount.startsWith('/_')) {
      continue;
    }
    allKnownMounts.push(mount);
    actualChecksums.set(mount, checksum);
    coordsKnownToBeGoodSources.set(mount, []);
    coordsKnownToBeBadSources.set(mount, new Map());
    if (!checksum || checksum !== safeChecksum(mount)) {
      checksumsINeedToFixLocally.push(mount);
    }
  }

  const serviceChecksumsByCoordinator = getChecksumsFromPeers(allKnownMounts);
  for (const [coordId, serviceChecksums] of serviceChecksumsByCoordinator) {
    for (const [mount, checksum] of serviceChecksums) {
      if (!checksum) {
        coordsKnownToBeBadSources.get(mount).set(coordId, null);
      } else if (!actualChecksums.get(mount)) {
        actualChecksums.set(mount, checksum);
        coordsKnownToBeGoodSources.get(mount).push(coordId);
      } else if (actualChecksums.get(mount) === checksum) {
        coordsKnownToBeGoodSources.get(mount).push(coordId);
      } else {
        coordsKnownToBeBadSources.get(mount).set(coordId, checksum);
      }
    }
  }

  const myId = getMyCoordinatorId();
  const serviceMountsToDeleteInCollection = [];
  const serviceChecksumsToUpdateInCollection = new Map();
  for (const mount of checksumsINeedToFixLocally) {
    const possibleSources = coordsKnownToBeGoodSources.get(mount);
    if (!possibleSources.length) {
      const myChecksum = safeChecksum(mount);
      if (myChecksum) {
        serviceChecksumsToUpdateInCollection.set(mount, myChecksum);
      } else {
        let found = false;
        for (const [coordId, coordChecksum] of coordsKnownToBeBadSources.get(mount)) {
          if (!coordChecksum) {
            continue;
          }
          const bundle = downloadServiceBundleFromCoordinator(coordId, mount, coordChecksum);
          if (bundle) {
            serviceChecksumsToUpdateInCollection.set(mount, coordChecksum);
            possibleSources.push(coordId);
            replaceLocalServiceFromTempBundle(mount, bundle);
            found = true;
            break;
          }
        }
        if (!found) {
          serviceMountsToDeleteInCollection.push(mount);
          coordsKnownToBeBadSources.delete(mount);
        }
      }
    } else {
      const checksum = actualChecksums.get(mount);
      for (const coordId of possibleSources) {
        const bundle = downloadServiceBundleFromCoordinator(coordId, mount, checksum);
        if (bundle) {
          replaceLocalServiceFromTempBundle(mount, bundle);
          break;
        }
      }
    }
  }

  for (const ids of coordsKnownToBeGoodSources.values()) {
    ids.push(myId);
  }

  db._query(aql`
    FOR service IN ${collection}
    FILTER service.mount IN ${serviceMountsToDeleteInCollection}
    REMOVE service
    IN ${collection}
  `);

  db._query(aql`
    FOR service IN ${collection}
    FOR item IN ${Array.from(serviceChecksumsToUpdateInCollection)}
    FILTER service.mount == item[0]
    UPDATE service
    WITH {checksum: item[1]}
    IN ${collection}
  `);

  parallelClusterRequests(function * () {
    for (const coordId of getPeerCoordinatorIds()) {
      const servicesYouNeedToUpdate = {};
      for (const [mount, badCoordinatorIds] of coordsKnownToBeBadSources) {
        if (!badCoordinatorIds.has(coordId)) {
          continue;
        }
        const goodCoordIds = coordsKnownToBeGoodSources.get(mount);
        servicesYouNeedToUpdate[mount] = shuffle(goodCoordIds);
      }
      if (!Object.keys(servicesYouNeedToUpdate).length) {
        continue;
      }
      yield [
        coordId,
        'POST',
        '/_api/foxx/_local',
        JSON.stringify(servicesYouNeedToUpdate),
        {'content-type': 'application/json'}
      ];
    }
  }());
}

// Change propagation

function reloadRouting () {
  require('internal').executeGlobalContextFunction('reloadRouting');
  actions.reloadRouting();
}

function propagateServiceDestroyed (service) {
  parallelClusterRequests(function * () {
    for (const coordId of getPeerCoordinatorIds()) {
      yield [coordId, 'DELETE', `/_api/foxx/_local/service?${querystringify({
        mount: service.mount
      })}`];
    }
  }());
  reloadRouting();
}

function propagateServiceReplaced (service) {
  const myId = getMyCoordinatorId();
  const coordIds = getPeerCoordinatorIds();
  const results = parallelClusterRequests(function * () {
    for (const coordId of coordIds) {
      yield [
        coordId,
        'POST',
        '/_api/foxx/_local',
        JSON.stringify({[service.mount]: [myId]}),
        {'content-type': 'application/json'}
      ];
    }
  }());
  for (const [coordId, result] of zip(coordIds, results)) {
    if (result.statusCode >= 400) {
      console.error(`Failed to propagate service ${service.mount} to coord ${coordId}`);
    }
  }
  reloadRouting();
}

function propagateServiceReconfigured (service) {
  parallelClusterRequests(function * () {
    for (const coordId of getPeerCoordinatorIds()) {
      yield [coordId, 'POST', `/_api/foxx/_local/service?${querystringify({
        mount: service.mount
      })}`];
    }
  }());
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
      const service = FoxxService.create(serviceDefinition);
      localServiceMap.set(service.mount, service);
    } catch (e) {
      if (fs.exists(FoxxService.basePath(serviceDefinition.mount))) {
        console.errorStack(e, `Failed to load service ${serviceDefinition.mount}`);
      } else {
        console.warn(`Could not find local service ${serviceDefinition.mount}`);
      }
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
  if (localServiceMap.has(mount)) {
    return localServiceMap.get(mount);
  }
  return reloadInstalledService(mount);
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
  const service = FoxxService.create(serviceDefinition);
  if (runSetup) {
    service.executeScript('setup');
  }
  GLOBAL_SERVICE_MAP.get(db._name()).set(mount, service);
  return service;
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
  const tempServicePath = fs.getTempFile('services', false);
  const tempBundlePath = fs.getTempFile('bundles', false);
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

function _install (mount, options = {}) {
  const collection = utils.getStorage();
  const service = FoxxService.create({
    mount,
    options,
    noisy: true
  });
  GLOBAL_SERVICE_MAP.get(db._name()).set(mount, service);
  if (options.setup !== false) {
    service.executeScript('setup');
  }
  service.updateChecksum();
  const serviceDefinition = service.toJSON();
  db._query(aql`
    UPSERT {mount: ${mount}}
    INSERT ${serviceDefinition}
    REPLACE ${serviceDefinition}
    IN ${collection}
  `);
  ensureServiceExecuted(service, true);
  return service;
}

function _uninstall (mount, options = {}) {
  let service;
  try {
    service = getServiceInstance(mount);
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
  db._query(aql`
    FOR service IN ${collection}
    FILTER service.mount == ${mount}
    REMOVE service IN ${collection}
  `);
  GLOBAL_SERVICE_MAP.get(db._name()).delete(mount);
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

function downloadServiceBundleFromCoordinator (coordId, mount, checksum) {
  const response = parallelClusterRequests([[
    coordId,
    'GET',
    `/_api/foxx/_local/bundle?${querystringify({mount})}`,
    null,
    checksum ? {'if-match': `"${checksum}"`} : undefined
  ]])[0];
  if (response.headers['x-arango-response-code'].startsWith('404')) {
    return null;
  }
  const filename = fs.getTempFile('bundles', true);
  fs.writeFileSync(filename, response.rawBody);
  return filename;
}

function extractServiceBundle (archive, targetPath) {
  const tempFolder = fs.getTempFile('services', false);
  fs.makeDirectory(tempFolder);
  fs.unzipFile(archive, tempFolder, false, true);

  let manifestPath;
  // find the manifest with the shortest path
  const filenames = fs.listTree(tempFolder).sort((a, b) => a.length - b.length);
  for (const filename of filenames) {
    if (filename === 'manifest.json' || filename.endsWith('/manifest.json')) {
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

  if (manifestPath.endsWith('/manifest.json')) {
    // service basePath is a subfolder of tempFolder
    // so tempFolder still exists and needs to be removed
    fs.removeDirectoryRecursive(tempFolder, true);
  }
}

function replaceLocalServiceFromTempBundle (mount, tempBundlePath) {
  const tempServicePath = fs.getTempFile('services', false);
  extractServiceBundle(tempBundlePath, tempServicePath);
  _buildServiceInPath(mount, tempServicePath, tempBundlePath);
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
  propagateServiceReplaced(service);
  return service;
}

function installLocal (mount, coordIds) {
  for (const coordId of coordIds) {
    const filename = downloadServiceBundleFromCoordinator(coordId, mount);
    if (filename) {
      replaceLocalServiceFromTempBundle(mount, filename);
      return true;
    }
  }
  return false;
}

function uninstallLocal (mount) {
  const servicePath = FoxxService.basePath(mount);
  if (fs.exists(servicePath)) {
    try {
      fs.removeDirectoryRecursive(servicePath, true);
    } catch (e) {
      console.warnStack(e);
    }
  }
  const bundlePath = FoxxService.bundlePath(mount);
  if (fs.exists(bundlePath)) {
    try {
      fs.remove(bundlePath);
    } catch (e) {
      console.warnStack(e);
    }
  }
}

function uninstall (mount, options = {}) {
  ensureFoxxInitialized();
  const service = _uninstall(mount, options);
  propagateServiceDestroyed(service);
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
  propagateServiceReplaced(service);
  return service;
}

function upgrade (serviceInfo, mount, options = {}) {
  ensureFoxxInitialized();
  const oldService = getServiceInstance(mount);
  const serviceOptions = oldService.toJSON().options;
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
  propagateServiceReplaced(service);
  return service;
}

function runScript (scriptName, mount, options) {
  let service = getServiceInstance(mount);
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
  if (service.isDevelopment) {
    service = reloadInstalledService(mount, true);
  }
  ensureServiceLoaded(mount);
  return require('@arangodb/foxx/mocha').run(service, options.reporter);
}

function enableDevelopmentMode (mount) {
  const service = getServiceInstance(mount);
  service.development(true);
  utils.updateService(mount, service.toJSON());
  propagateServiceReconfigured(service);
  return service;
}

function disableDevelopmentMode (mount) {
  const service = getServiceInstance(mount);
  service.development(false);
  createServiceBundle(mount);
  service.updateChecksum();
  utils.updateService(mount, service.toJSON());
  // Make sure setup changes from devmode are respected
  service.executeScript('setup');
  propagateServiceReplaced(service);
  return service;
}

function setConfiguration (mount, options = {}) {
  const service = getServiceInstance(mount);
  const warnings = service.applyConfiguration(options.configuration, options.replace);
  utils.updateService(mount, service.toJSON());
  propagateServiceReconfigured(service);
  return warnings;
}

function setDependencies (mount, options = {}) {
  const service = getServiceInstance(mount);
  const warnings = service.applyDependencies(options.dependencies, options.replace);
  utils.updateService(mount, service.toJSON());
  propagateServiceReconfigured(service);
  return warnings;
}

// Misc exported functions

function requireService (mount) {
  mount = '/' + mount.replace(/^\/+|\/+$/g, '');
  const service = getServiceInstance(mount);
  return ensureServiceExecuted(service, true).exports;
}

function getMountPoints () {
  ensureFoxxInitialized();
  return Array.from(GLOBAL_SERVICE_MAP.get(db._name()).keys());
}

function installedServices () {
  ensureFoxxInitialized();
  return Array.from(GLOBAL_SERVICE_MAP.get(db._name()).values());
}

function listJson () {
  ensureFoxxInitialized();
  const json = [];
  for (const service of GLOBAL_SERVICE_MAP.get(db._name()).values()) {
    json.push({
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
      scripts: service.getScripts()
    });
  }
  return json;
}

function safeChecksum (mount) {
  try {
    return FoxxService.checksum(mount);
  } catch (e) {
    return null;
  }
}

// Exports

exports._installLocal = installLocal;
exports._uninstallLocal = uninstallLocal;
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
exports.proxyToFoxxmaster = proxyToFoxxmaster;
exports._reloadRouting = reloadRouting;
exports.reloadInstalledService = reloadInstalledService;
exports.ensureRouted = ensureServiceLoaded;
exports.initializeFoxx = initLocalServiceMap;
exports.ensureFoxxInitialized = ensureFoxxInitialized;
exports.heal = healMyselfAndCoords;
exports._startup = startup;
exports._selfHeal = selfHeal;
exports._createServiceBundle = createServiceBundle;
exports._resetCache = () => GLOBAL_SERVICE_MAP.clear();
exports._mountPoints = getMountPoints;
exports._isClusterReady = isClusterReadyForBusiness;
exports.listJson = listJson;

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

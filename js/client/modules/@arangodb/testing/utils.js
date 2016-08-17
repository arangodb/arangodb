'use strict';

const fs = require('fs');
const testPort = require('internal').testPort;
const endpointToURL = require('@arangodb/common.js').endpointToURL;
const toArgv = require('internal').toArgv;
const executeExternal = require('internal').executeExternal;
const platform = require('internal').platform;
const _ = require('lodash');
const base64Encode = require('internal').base64Encode;

let TOP_DIR = findTopDir();
let BIN_DIR = fs.join(TOP_DIR, 'build', 'bin');
let ARANGOD_BIN = fs.join(BIN_DIR, 'arangod');
  

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a command, possible with valgrind
// //////////////////////////////////////////////////////////////////////////////

function executeArangod (cmd, args, options) {
  if (options.valgrind) {
    let valgrindOpts = {};

    if (options.valgrindArgs) {
      valgrindOpts = options.valgrindArgs;
    }

    let testfn = options.valgrindFileBase;

    if (testfn.length > 0) {
      testfn += '_';
    }

    if (valgrindOpts.xml === 'yes') {
      valgrindOpts['xml-file'] = testfn + '.%p.xml';
    }

    valgrindOpts['log-file'] = testfn + '.%p.valgrind.log';

    args = toArgv(valgrindOpts, true).concat([cmd]).concat(args);
    cmd = options.valgrind;
  } else if (options.rr) {
    args = [cmd].concat(args);
    cmd = 'rr';
  }

  if (options.extremeVerbosity) {
    print('starting process ' + cmd + ' with arguments: ' + JSON.stringify(args));
  }
  return executeExternal(cmd, args);
}

function startArango (protocol, options, addArgs, rootDir, role) {
  const dataDir = fs.join(rootDir, 'data');
  const appDir = fs.join(rootDir, 'apps');

  fs.makeDirectoryRecursive(dataDir);
  fs.makeDirectoryRecursive(appDir);

  let args = makeArgsArangod(options, appDir);
  let endpoint;
  let port;

  if (!addArgs['server.endpoint']) {
    port = findFreePort(options.maxPort);
    endpoint = protocol + '://127.0.0.1:' + port;
  } else {
    endpoint = addArgs['server.endpoint'];
    port = endpoint.split(':').pop();
  }

  let instanceInfo = {
  role, port, endpoint, rootDir};

  args['server.endpoint'] = endpoint;
  args['database.directory'] = dataDir;
  args['log.file'] = fs.join(rootDir, 'log');

  if (options.verbose) {
    args['log.level'] = 'info';
  } else {
    args['log.level'] = 'error';
  }

  // flush log messages directly and not asynchronously
  // (helps debugging)  
  args['log.force-direct'] = 'true';

  if (protocol === 'ssl') {
    args['ssl.keyfile'] = fs.join('UnitTests', 'server.pem');
  }

  args = Object.assign(args, options.extraArgs);

  if (addArgs !== undefined) {
    args = Object.assign(args, addArgs);
  }

  instanceInfo.url = endpointToURL(instanceInfo.endpoint);
  instanceInfo.args = toArgv(args);
  instanceInfo.options = options;
  instanceInfo.pid = executeArangod(ARANGOD_BIN, instanceInfo.args, options).pid;
  instanceInfo.role = role;

  if (platform.substr(0, 3) === 'win') {
    const procdumpArgs = [
      '-accepteula',
      '-e',
      '-ma',
      instanceInfo.pid,
      fs.join(rootDir, 'core.dmp')
    ];

    try {
      instanceInfo.monitor = executeExternal('procdump', procdumpArgs);
    } catch (x) {
      print('failed to start procdump - is it installed?');
      throw x;
    }
  }
  return instanceInfo;
}

function findTopDir () {
  const topDir = fs.normalize(fs.makeAbsolute('.'));

  if (!fs.exists('3rdParty') && !fs.exists('arangod') &&
    !fs.exists('arangosh') && !fs.exists('UnitTests')) {
    throw 'Must be in ArangoDB topdir to execute unit tests.';
  }

  return topDir;
}

function startInstanceAgency(instanceInfo, protocol, options,
  addArgs, rootDir) {
  console.error("HASS", instanceInfo);
  const dataDir = fs.join(rootDir, 'data');

  const N = options.agencySize;
  if (options.agencyWaitForSync === undefined) {
    options.agencyWaitForSync = false;
  }
  const wfs = options.agencyWaitForSync;

  for (let i = 0; i < N; i++) {
    let instanceArgs = _.clone(addArgs);
    instanceArgs['agency.id'] = String(i);
    instanceArgs['agency.size'] = String(N);
    instanceArgs['agency.wait-for-sync'] = String(wfs);
    instanceArgs['agency.supervision'] = 'true';
    instanceArgs['database.directory'] = dataDir + String(i);

    if (i === N - 1) {
      const port = findFreePort(options.maxPort);
      instanceArgs['server.endpoint'] = 'tcp://127.0.0.1:' + port;
      let l = [];
      instanceInfo.arangods.forEach(arangod => {
        l.push('--agency.endpoint');
        l.push(arangod.endpoint);
      });
      l.push('--agency.endpoint');
      l.push('tcp://127.0.0.1:' + port);
      l.push('--agency.notify');
      l.push('true');

      instanceArgs['flatCommands'] = l;
    }
    let dir = fs.join(rootDir, 'agency-' + i);
    fs.makeDirectoryRecursive(dir);

    instanceInfo.arangods.push(startArango(protocol, options, instanceArgs, rootDir, 'agent'));
  }

  instanceInfo.endpoint = instanceInfo.arangods[instanceInfo.arangods.length - 1].endpoint;
  instanceInfo.url = instanceInfo.arangods[instanceInfo.arangods.length - 1].url;
  instanceInfo.role = 'agent';
  print('Agency Endpoint: ' + instanceInfo.endpoint);

  return instanceInfo;
}


// //////////////////////////////////////////////////////////////////////////////
// / @brief arguments for testing (server)
// //////////////////////////////////////////////////////////////////////////////

function makeArgsArangod (options, appDir) {
  if (appDir === undefined) {
    appDir = fs.getTempPath();
  }

  fs.makeDirectoryRecursive(appDir, true);

  return {
    'configuration': 'none',
    'database.force-sync-properties': 'false',
    'database.maximal-journal-size': '1048576',
    'javascript.app-path': appDir,
    'javascript.startup-directory': 'js',
    'javascript.v8-contexts': '5',
    'http.trusted-origin': options.httpTrustedOrigin || 'all',
    'log.level': 'warn',
    'log.level=replication=warn': null,
    'server.allow-use-database': 'true',
    'server.authentication': 'false',
    'server.threads': '20',
    'ssl.keyfile': 'UnitTests/server.pem',
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief finds a free port
// //////////////////////////////////////////////////////////////////////////////

function findFreePort (maxPort) {
  if (typeof maxPort !== 'number') {
    maxPort = 32768;
  }
  if (maxPort < 2048) {
    maxPort = 2048;
  }
  while (true) {
    const port = Math.floor(Math.random() * (maxPort - 1024)) + 1024;
    const free = testPort('tcp://0.0.0.0:' + port);

    if (free) {
      return port;
    }
  }

  return 8529;
}

exports.findTopDir = findTopDir;
exports.findFreePort = findFreePort;
exports.startInstanceAgency = startInstanceAgency;
exports.startArango = startArango;
exports.executeArangod = executeArangod;
exports.makeArgsArangod = makeArgsArangod;

exports.startAgency = function(size, options = {}) {
  let instances = [];
  options.agencySize = size;
  let legacyInstanceInfo = {arangods: []};
  for (var i=0;i<size;i++) {
    instances.push(startInstanceAgency(legacyInstanceInfo, 'tcp', options, {}, 'agency-' + i), options.tmpDir);
  }
  return instances;
};
exports.startCoordinator = function(rootDir, agencyEndpoint, options = {}) {
};
exports.startDBServer = function(rootDir, agencyEndpoint, options = {}) {
};
exports.checkInstance = function() {};

exports.createRootDir = function() {
}

exports.makeAuthorizationHeaders = function(options) {
  return {
    'headers': {
      'Authorization': 'Basic ' + base64Encode(options.username + ':' +
          options.password)
    }
  };
}
exports.ARANGOD_BIN = ARANGOD_BIN;

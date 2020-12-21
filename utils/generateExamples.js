/*jshint globalstrict:false, unused:false */
/*global start_pretty_print */
'use strict';

const _ = require("lodash");
const fs = require("fs");
const internal = require("internal");
const pu = require('@arangodb/testutils/process-utils');
const executeExternal = internal.executeExternal;
const executeExternalAndWait = internal.executeExternalAndWait;
const download = internal.download;
const print = internal.print;
const wait = internal.wait;
const killExternal = internal.killExternal;
const toArgv = internal.toArgv;
const statusExternal = internal.statusExternal;
const testPort = internal.testPort;

const yaml = require("js-yaml");

const optionsDefaults = {
  'dumpAgencyOnError': false,
  'agencySize': 3,
  'agencyWaitForSync': false,
  'agencySupervision': true,
  'build': '',
  'buildType': '',
  'cleanup': true,
  'cluster': false,
  'concurrency': 3,
  'configDir': 'etc/testing',
  'coordinators': 1,
  'coreCheck': false,
  'coreDirectory': '/var/tmp',
  'dbServers': 2,
  'duration': 10,
  'extraArgs': {},
  'extremeVerbosity': true, /// TODO false,
  'force': true,
  'getSockStat': false,
  'arangosearch':true,
  'jsonReply': false,
  'minPort': 1024,
  'maxPort': 32768,
  'password': '',
  'protocol': 'tcp',
  'replication': false,
  'rr': false,
  'exceptionFilter': null,
  'exceptionCount': 1,
  'sanitizer': false,
  'activefailover': false,
  'singles': 2,
  'sniff': false,
  'sniffDevice': undefined,
  'sniffProgram': undefined,
  'skipLogAnalysis': true,
  'oneTestTimeout': 2700,
  'isAsan': false,
  'storageEngine': 'rocksdb',
  'test': undefined,
  'testBuckets': undefined,
  'useReconnect': true,
  'username': 'root',
  'valgrind': false,
  'valgrindFileBase': '',
  'valgrindArgs': {},
  'valgrindHosts': false,
  'verbose': false,
  'walFlushTimeout': 30000,
  'testCase': undefined,
  'disableMonitor': false,
  'sleepBeforeStart' : 0,
};


const documentationSourceDirs = [
  fs.join(fs.makeAbsolute(''), "Documentation/Scripts/setup-arangosh.js"),
  fs.join(fs.makeAbsolute(''), "Documentation/DocuBlocks"),
  fs.join(fs.makeAbsolute(''), "Documentation/Books/Manual"),
  fs.join(fs.makeAbsolute(''), "Documentation/Books/AQL"),
  fs.join(fs.makeAbsolute(''), "Documentation/Books/HTTP")
];

const theScript = 'utils/generateExamples.py';

const scriptArguments = {
  'outputDir': fs.join(fs.makeAbsolute(''), "Documentation/Examples"),
  'outputFile': fs.join(fs.makeAbsolute(''), "arangosh.examples.js")
};

const programPaths = [
  "build/bin/",
  "build/bin/RelWithDebInfo/",
  "bin/"
];

let ARANGOD;
let ARANGOSH;

function locateProgram(programName, errMsg) {
  for (let programPath of programPaths) {
    let path = fs.join(fs.join(fs.makeAbsolute('')), programPath, programName);
    if (fs.isFile(path) || fs.isFile(path + ".exe")) {
      return path;
    }
  }
  throw errMsg;
}

function endpointToURL(endpoint) {
  if (endpoint.substr(0, 6) === "ssl://") {
    return "https://" + endpoint.substr(6);
  }

  const pos = endpoint.indexOf("://");

  if (pos === -1) {
    return "http://" + endpoint;
  }

  return "http" + endpoint.substr(pos);
}

function findFreePort() {
  while (true) {
    const port = Math.floor(Math.random() * (65536 - 1024)) + 1024;
    const free = testPort("tcp://0.0.0.0:" + port);

    if (free) {
      return port;
    }
  }

  return 8529;
}

function main(argv) {
  let thePython = 'python';
  if (fs.exists('build/CMakeCache.txt')) {
    let CMakeCache = fs.readFileSync('build/CMakeCache.txt');
    thePython = CMakeCache.toString().match(/^PYTHON_EXECUTABLE:FILEPATH=(.*)$/m)[1];
  }
  let options = {};
  let serverEndpoint = '';
  let startServer = true;
  let instanceInfo = {};
  let serverCrashed = false;
  let protocol = 'tcp';
  let tmpDataDir = fs.getTempFile();
  let count = 0;

  try {
    options = internal.parseArgv(argv, 0);
  } catch (x) {
    print("failed to parse the options: " + x.message);
    return -1;
  }
  if (options.hasOwnProperty('withPython')) {
    thePython = options.withPython;
  }

  if (options.hasOwnProperty('onlyThisOne')) {
    scriptArguments.onlyThisOne = options.onlyThisOne;
  }
  if (options.hasOwnProperty('test')) {
    scriptArguments.onlyThisOne = options.test;
  }

  if (options.hasOwnProperty('server.endpoint')) {
    if (scriptArguments.hasOwnProperty('onlyThisOne')) {
      throw("don't run the full suite on pre-existing servers");
    }
    startServer = false;
    serverEndpoint = options['server.endpoint'];
    
  }
  _.defaults(options, optionsDefaults);

  try {
    pu.setupBinaries(options.build, options.buildType, options.configDir);
  }
  catch (err) {
    print(err);
    throw err;
  }

  let args = [theScript].concat(internal.toArgv(scriptArguments));
  args = args.concat(['--arangoshSetup']);
  args = args.concat(documentationSourceDirs);

  let storageEngines = [['rocksdb', true, false], ['rocksdb', true, true]];
  let res;

  storageEngines.forEach(function (engine) {
    let pyArgs = _.clone(args);
    pyArgs.push('--storageEngine');
    pyArgs.push(engine[0]);
    pyArgs.push('--storageEngineAgnostic');
    pyArgs.push(engine[1]);
    pyArgs.push('--cluster');
    pyArgs.push(engine[2]);
    print(pyArgs)
    res = executeExternalAndWait(thePython, pyArgs);

    if (res.exit !== 0) {
      print("parsing the examples failed - aborting!");
      print(res);
      return -1;
    }
    let instanceInfo;
    if (startServer) {
      // let port = findFreePort();
      options.storageEngine = engine[0];
      options.cluster = engine[2];
      let testname = engine[0] + "-" + "clusterOrNot";
      print(options)
      instanceInfo = pu.startInstance(options.protocol, options, {}, testname);
      print(instanceInfo)
    }
    
    let arangoshArgs = {
      'configuration': fs.join(fs.makeAbsolute(''), 'etc', 'relative', 'arangosh.conf'),
      'server.password': "",
      'server.endpoint': instanceInfo.endpoint,
      'javascript.execute': scriptArguments.outputFile
    };

    print("--------------------------------------------------------------------------------");
    print(pu.ARANGOSH_BIN);
    print(internal.toArgv(arangoshArgs));
    res = executeExternalAndWait(pu.ARANGOSH_BIN, internal.toArgv(arangoshArgs));
    if (startServer) {
      pu.shutdownInstance(instanceInfo, options);
    }
    if (res.exit != 0) {
      throw("generating examples failed - aborting!");
    }
  });

  return 0;
}

main(ARGUMENTS);

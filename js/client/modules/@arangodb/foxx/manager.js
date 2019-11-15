/* jshint unused: false */
/* global ARANGODB_CLIENT_VERSION */ 
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// / Copyright 2013 triAGENS GmbH, Cologne, Germany
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
// / @author Jan Steemann
// / @author Michael Hackstein
// / @author Dr. Frank Celler
// //////////////////////////////////////////////////////////////////////////////

var internal = require('internal');
var arangodb = require('@arangodb');
var arangosh = require('@arangodb/arangosh');
var errors = arangodb.errors;
var ArangoError = arangodb.ArangoError;
var checkParameter = arangodb.checkParameter;
var arango = require('internal').arango;
var fs = require('fs');

var throwFileNotFound = arangodb.throwFileNotFound;
var throwBadParameter = arangodb.throwBadParameter;

var utils = require('@arangodb/foxx/manager-utils');
var store = require('@arangodb/foxx/store');

// //////////////////////////////////////////////////////////////////////////////
// / @brief extracts command-line options
// //////////////////////////////////////////////////////////////////////////////

var extractCommandLineOptions = function (args) {
  var options = {};
  var nargs = [];
  var i;

  var reOption = /^([\-_a-zA-Z0-9]*)=(.*)$/;
  var reNumeric = /^(0|.0|([0-9]*(\.[0-9]*)?))$/;
  var arg, match, key, value;

  for (i = 0;  i < args.length;  ++i) {
    arg = args[i];
    match = reOption.exec(arg);

    if (match !== null) {
      key = match[1];
      value = match[2];

      if (reNumeric.test(value)) {
        options[key] = parseFloat(value);
      } else {
        options[key] = value;
      }
    } else {
      nargs.push(args[i]);
    }
  }

  return { 'options': options, 'args': nargs };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief extract options from CLI options
// //////////////////////////////////////////////////////////////////////////////
var extractOptions = function (args) {
  var opts = extractCommandLineOptions(args);
  if (3 < opts.args.length) {
    var options = JSON.parse(opts.args[3]);

    if (options.hasOwnProperty('configuration')) {
      var key;

      for (key in opts.options) {
        if (opts.options.hasOwnProperty(key)) {
          options.configuration[key] = opts.options[key];
        }
      }
    } else {
      options.configuration = opts.options;
    }

    return options;
  }

  return { configuration: opts.options };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief prints out usage message for the command-line tool
// //////////////////////////////////////////////////////////////////////////////

var cmdUsage = function () {
  var printf = arangodb.printf;
  var fm = 'foxx-manager';

  printf('Example usage:\n');
  printf(' %s install <service-info> <mount-point> option1=value1\n', fm);
  printf(' %s uninstall <mount-point>\n\n', fm);

  printf('Further help:\n');
  printf(' %s help   for the list of foxx-manager commands\n', fm);
  printf(' %s --help for the list of options\n', fm);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief outputs the help
// //////////////////////////////////////////////////////////////////////////////

var help = function () {

  /* jshint maxlen: 200 */
  var commands = {
    'available': 'lists all Foxx services available in the local repository',
    'configuration': 'request the configuration information for the given mount point',
    'configure': 'sets the configuration for the given mount point',
    'updateDeps': 'links the dependencies in manifest to a mount point',
    'dependencies': 'request the dependencies information for the given mount point',
    'development': 'activates development mode for the given mount point',
    'help': 'shows this help',
    'info': 'displays information about a Foxx service',
    'install': 'installs a Foxx service identified by the given information to the given mount point',
    'installed': "alias for the 'list' command",
    'list': 'lists all installed Foxx services',
    'production': 'activates production mode for the given mount point',
    'replace': ['replaces an installed Foxx service',
      'WARNING: this action will remove service data if the service implements teardown!' ],
    'run': 'runs the given script of a Foxx service mounted at the given mount point',
    'search': 'searches the local foxx-apps repository',
    'set-dependencies': 'sets the dependencies for the given mount point',
    'setup': 'executes the setup script',
    'teardown': [ 'executes the teardown script',
      'WARNING: this action will remove service data if the service implements teardown!' ],
    'tests': 'runs the tests of a Foxx service mounted at the given mount point',
    'uninstall': ['uninstalls a Foxx service and calls its teardown method',
      'WARNING: this will remove all data and code of the service!' ],
    'update': 'updates the local foxx-apps repository with data from the central foxx-apps repository',
    'upgrade': ['upgrades an installed Foxx service',
      'Note: this action will not call setup or teardown' ]
  };

  arangodb.print('\nThe following commands are available:\n');
  var keys = Object.keys(commands).sort();

  var i;
  var pad, name, extra;
  for (i = 0; i < keys.length; ++i) {
    pad = '                  ';
    name = keys[i] + pad;
    extra = commands[keys[i]];

    if (typeof extra !== 'string') {
      // list of strings
      extra = extra.join('\n  ' + pad) + '\n';
    }
    arangodb.printf(' %s %s\n', name.substr(0, pad.length), extra);
  }

  arangodb.print();
  arangodb.print('Use  foxx-manager --help  to show a list of global options\n');
  arangodb.print('There is also an online manual available at:');

  // extract current version
  var version;
  try {
    version = ARANGODB_CLIENT_VERSION().replace(/^(\d+\.\d+).*/, "$1");
  } catch (err) {}
  if (!version.match(/^(\d+\.\d+)$/)) {
    version = 'stable';
  }
  arangodb.print('https://www.arangodb.com/docs/' + version + '/foxx.html');

  // additional newline
  arangodb.print();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs a script on a Foxx service
// /
// / Input:
// / * name: the name of the script
// / * mount: the mount path starting with a "/"
// /
// / Output:
// / -
// //////////////////////////////////////////////////////////////////////////////
var runScript = function (mount, name, options) {
  checkParameter(
    'run(<mount>, <name>, [<options>])',
    [ [ 'Mount path', 'string' ], [ 'Script name', 'string' ] ],
    [ mount, name ]);
  var res;
  var req = {
    name: name,
    mount: mount,
    options: options
  };
  res = arango.POST('/_admin/foxx/script', req);
  arangosh.checkRequestResult(res);
  return res;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Zips and copies a local service to the server.
// //////////////////////////////////////////////////////////////////////////////

var moveAppToServer = function (serviceInfo) {
  if (!fs.exists(serviceInfo)) {
    throwFileNotFound('Cannot find file: ' + serviceInfo + '.');
  }
  var filePath;
  var shouldDelete = false;
  if (fs.isDirectory(serviceInfo)) {
    filePath = utils.zipDirectory(serviceInfo);
    shouldDelete = true;
  }
  if (fs.isFile(serviceInfo)) {
    filePath = serviceInfo;
  }
  if (!filePath) {
    throwBadParameter('Invalid file: ' + serviceInfo + '. Has to be a direcotry or zip archive');
  }
  var response = arango.SEND_FILE('/_api/upload', filePath, {'content-type': 'application/zip'});
  if (shouldDelete) {
    try {
      fs.remove(filePath);
    } catch (err2) {
      arangodb.printf("Cannot remove temporary file '%s'\n", filePath);
    }
  }
  if (!response.filename) {
    throw new ArangoError({
      errorNum: errors.ERROR_SERVICE_UPLOAD_FAILED.code,
      errorMessage: errors.ERROR_SERVICE_UPLOAD_FAILED.message
        + ': ' + String(response.errorMessage)
    });
  }
  return response.filename;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Installs a new Foxx service on the given mount point.
// /
// / TODO: Long Documentation!
// //////////////////////////////////////////////////////////////////////////////

var install = function (serviceInfo, mount, options) {
  checkParameter(
    'install(<serviceInfo>, <mount>, [<options>])',
    [ [ 'Install information', 'string' ],
      [ 'Mount path', 'string' ] ],
    [ serviceInfo, mount ]);

  utils.validateMount(mount);
  if (fs.exists(serviceInfo)) {
    serviceInfo = moveAppToServer(serviceInfo);
  }
  var res;
  var req = {
    appInfo: serviceInfo,
    mount: mount,
    options: options
  };

  res = arango.POST('/_admin/foxx/install', req);
  arangosh.checkRequestResult(res);
  return {
    name: res.name,
    version: res.version,
    mount: res.mount
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Uninstalls the Foxx service on the given mount point.
// /
// / TODO: Long Documentation!
// //////////////////////////////////////////////////////////////////////////////

var uninstall = function (mount, options) {
  checkParameter(
    'uninstall(<mount>, [<options>])',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);
  var res;
  var req = {
    mount: mount,
    options: options || {}
  };
  utils.validateMount(mount);
  res = arango.POST('/_admin/foxx/uninstall', req);
  arangosh.checkRequestResult(res);
  return {
    name: res.name,
    version: res.version,
    mount: res.mount
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Replaces a Foxx service on the given mount point by an other one.
// /
// / TODO: Long Documentation!
// //////////////////////////////////////////////////////////////////////////////

var replace = function (serviceInfo, mount, options) {
  checkParameter(
    'replace(<serviceInfo>, <mount>, [<options>])',
    [ [ 'Install information', 'string' ],
      [ 'Mount path', 'string' ] ],
    [ serviceInfo, mount ]);

  utils.validateMount(mount);
  if (fs.exists(serviceInfo)) {
    serviceInfo = moveAppToServer(serviceInfo);
  }
  var res;
  var req = {
    appInfo: serviceInfo,
    mount: mount,
    options: options
  };

  res = arango.POST('/_admin/foxx/replace', req);
  arangodb.db._flushCache();
  arangosh.checkRequestResult(res);
  return {
    name: res.name,
    version: res.version,
    mount: res.mount
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Upgrade a foxx service on the given mount point by a new one.
// /
// / TODO: Long Documentation!
// //////////////////////////////////////////////////////////////////////////////

var upgrade = function (serviceInfo, mount, options) {
  checkParameter(
    'upgrade(<serviceInfo>, <mount>, [<options>])',
    [ [ 'Install information', 'string' ],
      [ 'Mount path', 'string' ] ],
    [ serviceInfo, mount ]);
  utils.validateMount(mount);
  if (fs.exists(serviceInfo)) {
    serviceInfo = moveAppToServer(serviceInfo);
  }
  var res;
  var req = {
    appInfo: serviceInfo,
    mount: mount,
    options: options
  };

  res = arango.POST('/_admin/foxx/upgrade', req);
  arangosh.checkRequestResult(res);
  return {
    name: res.name,
    version: res.version,
    mount: res.mount
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Activate the development mode for the service on the given mount point.
// //////////////////////////////////////////////////////////////////////////////

var development = function (mount) {
  checkParameter(
    'development(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);
  utils.validateMount(mount);
  var res;
  var req = {
    mount: mount,
    activate: true
  };
  res = arango.POST('/_admin/foxx/development', req);
  arangosh.checkRequestResult(res);
  return {
    name: res.name,
    version: res.version,
    mount: res.mount
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Activate the production mode for the service on the given mount point.
// //////////////////////////////////////////////////////////////////////////////

var production = function (mount) {
  checkParameter(
    'production(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);
  utils.validateMount(mount);
  var res;
  var req = {
    mount: mount,
    activate: false
  };
  res = arango.POST('/_admin/foxx/development', req);
  arangosh.checkRequestResult(res);
  return {
    name: res.name,
    version: res.version,
    mount: res.mount
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Configure the service at the mount point
// //////////////////////////////////////////////////////////////////////////////

var configure = function (mount, options) {
  checkParameter(
    'configure(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);
  utils.validateMount(mount);
  var req = {
    mount: mount,
    options: options
  };
  var res = arango.POST('/_admin/foxx/configure', req);
  arangosh.checkRequestResult(res);
  return {
    name: res.name,
    version: res.version,
    mount: res.mount
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Get the configuration for the service at the given mount point
// //////////////////////////////////////////////////////////////////////////////

var configuration = function (mount) {
  checkParameter(
    'configuration(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);
  utils.validateMount(mount);
  var req = {
    mount: mount
  };
  var res = arango.POST('/_admin/foxx/configuration', req);
  arangosh.checkRequestResult(res);
  return res;
};
// //////////////////////////////////////////////////////////////////////////////
// / @brief Link Dependencies to the installed mount point the service at the mount point
// //////////////////////////////////////////////////////////////////////////////

var updateDeps = function (mount, options) {
  checkParameter(
    'update(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);
  utils.validateMount(mount);
  var req = {
    mount: mount,
    options: options
  };
  var res = arango.POST('/_admin/foxx/updateDeps', req);
  arangosh.checkRequestResult(res);
  return {
    name: res.name,
    version: res.version,
    mount: res.mount
  };
};
// //////////////////////////////////////////////////////////////////////////////
// / @brief Configure the dependencies of the service at the mount point
// //////////////////////////////////////////////////////////////////////////////

var setDependencies = function (mount, options) {
  checkParameter(
    'setDependencies(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);
  utils.validateMount(mount);
  var req = {
    mount: mount,
    options: options
  };
  var res = arango.POST('/_admin/foxx/set-dependencies', req);
  arangosh.checkRequestResult(res);
  return {
    name: res.name,
    version: res.version,
    mount: res.mount
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief Get the dependencies of the service at the given mount point
// //////////////////////////////////////////////////////////////////////////////

var dependencies = function (mount) {
  checkParameter(
    'dependencies(<mount>)',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]);
  utils.validateMount(mount);
  var req = {
    mount: mount
  };
  var res = arango.POST('/_admin/foxx/dependencies', req);
  arangosh.checkRequestResult(res);
  return res;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief run a Foxx service's tests
// //////////////////////////////////////////////////////////////////////////////

var tests = function (mount, options) {
  checkParameter(
    'tests(<mount>, [<options>])',
    [ [ 'Mount path', 'string' ] ],
    [ mount ]
  );
  utils.validateMount(mount);
  var req = {
    mount: mount,
    options: options
  };
  var res = arango.POST('/_admin/foxx/tests', req);
  arangosh.checkRequestResult(res);
  return res;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief command line dispatcher
// //////////////////////////////////////////////////////////////////////////////

var run = function (args) {
  var version = internal.version.split('.').slice(0, 2).join('.');
  arangodb.print('NOTE: foxx-manager is deprecated and will be removed in ArangoDB v4.0.');
  arangodb.print(`Please use foxx-cli instead: https://www.arangodb.com/docs/${version}/programs-foxx-cli.html\n`);

  if (args === undefined || args.length === 0) {
    arangodb.print('Expecting a command, please try:\n');
    cmdUsage();
    return 0;
  }

  var type = args[0];
  var printf = arangodb.printf;
  var res;
  var options;

  try {
    switch (type) {
      case 'setup':
        runScript(args[1], 'setup');
        break;
      case 'teardown':
        runScript(args[1], 'teardown');
        break;
      case 'run':
        options = args.slice(3).map(function (arg) {
          return JSON.parse(arg);
        });
        res = runScript(args[1], args[2], options);
        printf(JSON.stringify(res, null, 2) + '\n');
        break;
      case 'tests':
        options = args[2] ? JSON.parse(args[2]) : undefined;
        res = tests(args[1], options);
        printf(JSON.stringify(res, null, 2) + '\n');
        break;
      case 'install':
        options = extractOptions(args);
        res = install(args[1], args[2], options);
        printf('Service %s version %s installed successfully at mount point %s\n',
          res.name,
          res.version,
          res.mount);
        printf('options used: %s\n', JSON.stringify(options));
        break;
      case 'replace':
        options = extractOptions(args);
        res = replace(args[1], args[2], options);
        printf('Service %s version %s replaced successfully at mount point %s\n',
          res.name,
          res.version,
          res.mount);
        break;
      case 'upgrade':
        options = extractOptions(args);
        res = upgrade(args[1], args[2], options);
        printf('Service %s version %s upgraded successfully at mount point %s\n',
          res.name,
          res.version,
          res.mount);
        break;
      case 'uninstall':
        options = extractOptions(args).configuration || {};
        res = uninstall(args[1], options);

        printf('Service %s version %s uninstalled successfully from mount point %s\n',
          res.name,
          res.version,
          res.mount);
        break;
      case 'list':
      case 'installed':
        if (1 < args.length && args[1] === 'prefix') {
          utils.list(true);
        } else {
          utils.list();
        }
        break;
      case 'listDevelopment':
        if (1 < args.length && args[1] === 'prefix') {
          utils.listDevelopment(true);
        } else {
          utils.listDevelopment();
        }
        break;
      case 'available':
        store.available();
        break;
      case 'info':
        store.info(args[1]);
        break;
      case 'search':
        store.search(args[1]);
        break;
      case 'update':
        store.update();
        break;
      case 'help':
        help();
        break;
      case 'development':
        res = development(args[1]);
        printf('Activated development mode for Service %s version %s on mount point %s\n',
          res.name,
          res.version,
          res.mount);
        break;
      case 'production':
        res = production(args[1]);
        printf('Activated production mode for Service %s version %s on mount point %s\n',
          res.name,
          res.version,
          res.mount);
        break;
      case 'configure':
        options = extractOptions(args).configuration || {};
        res = configure(args[1], options);
        printf('Reconfigured Service %s version %s on mount point %s\n',
          res.name,
          res.version,
          res.mount);
        break;
      case 'configuration':
        res = configuration(args[1]);
        printf('Configuration options:\n%s\n', JSON.stringify(res, undefined, 2));
        break;
      case 'updateDeps':
        options = extractOptions(args).configuration || {};
        res = updateDeps(args[1], options);
        printf('Reconfigured Service %s version %s on mount point %s\n',
          res.name,
          res.version,
          res.mount);
        break;
      case 'set-dependencies':
        options = extractOptions(args).configuration || {};
        res = setDependencies(args[1], options);
        printf('Reconfigured dependencies of Service %s version %s on mount point %s\n',
          res.name,
          res.version,
          res.mount);
        break;
      case 'dependencies':
        res = dependencies(args[1]);
        printf('Dependencies:\n%s\n', JSON.stringify(res, undefined, 2));
        break;
      default:
        printf("Unknown command '%s', please try:\n", type);
        cmdUsage();
    }
    return 0;
  } catch (err) {
    arangodb.print(String(err));
    return 1;
  }
};

exports.install = install;
exports.setup = function (mount, opts) {return runScript(mount, 'setup', opts);};
exports.teardown = function (mount, opts) {return runScript(mount, 'teardown', opts);};
exports.runScript = runScript;
exports.tests = tests;
exports.uninstall = uninstall;
exports.replace = replace;
exports.upgrade = upgrade;
exports.development = development;
exports.production = production;
exports.configure = configure;
exports.configuration = configuration;
exports.setDependencies = setDependencies;
exports.dependencies = dependencies;

// //////////////////////////////////////////////////////////////////////////////
// / @brief Clientside only API
// //////////////////////////////////////////////////////////////////////////////

exports.run = run;
exports.help = help;

// //////////////////////////////////////////////////////////////////////////////
// / @brief Exports from Foxx utils module.
// //////////////////////////////////////////////////////////////////////////////

exports.list = utils.list;
exports.listDevelopment = utils.listDevelopment;
exports.listDevelopmentJson = utils.listDevelopmentJson;

// //////////////////////////////////////////////////////////////////////////////
// / @brief Exports from Foxx store module.
// //////////////////////////////////////////////////////////////////////////////

exports.available = store.available;
exports.availableJson = store.availableJson;
exports.search = store.search;
exports.searchJson = store.searchJson;
exports.update = store.update;
exports.info = store.info;

'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief basic initialization
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

(function () {
  let startupPath = global.STARTUP_PATH;
  let load = global.SYS_LOAD;

  if (startupPath === '') {
    startupPath = '.';
  }

  load(`${startupPath}/common/bootstrap/scaffolding.js`);
  load(`${startupPath}/common/bootstrap/modules/internal.js`); // deps: -
  load(`${startupPath}/common/bootstrap/errors.js`); // deps: internal
  load(`${startupPath}/common/bootstrap/modules/console.js`); // deps: internal
  load(`${startupPath}/common/bootstrap/modules/assert.js`); // deps: -
  load(`${startupPath}/common/bootstrap/modules/buffer.js`); // deps: internal
  load(`${startupPath}/common/bootstrap/modules/fs.js`); // deps: internal, buffer (hidden)
  load(`${startupPath}/common/bootstrap/modules/path.js`); // deps: internal, fs
  load(`${startupPath}/common/bootstrap/modules/events.js`); // deps: -
  load(`${startupPath}/common/bootstrap/modules/process.js`); // deps: internal, fs, console, events
  load(`${startupPath}/server/bootstrap/modules/internal.js`); // deps: internal, fs, console
  load(`${startupPath}/common/bootstrap/modules/vm.js`); // deps: internal
  load(`${startupPath}/common/bootstrap/modules.js`); // must come last before patches
}());

// common globals
global.console = require('console');
global.Buffer = require('buffer').Buffer;
global.process = require('process');
global.setInterval = function () {};
global.clearInterval = function () {};
global.setTimeout = function () {};
global.clearTimeout = function () {};

// template string generator for building an AQL query
global.aqlQuery = require('@arangodb').aql;

// load the actions from the actions directory
require('@arangodb/actions').startup();

// initialize AQL
if (global._AQL === undefined) { 
  global._AQL = require('@arangodb/aql'); 
}

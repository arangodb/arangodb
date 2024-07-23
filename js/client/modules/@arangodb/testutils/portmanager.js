/* jshint strict: false, sub: true */
/* global print, arango */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////
const internal = require('internal');
const tu = require('@arangodb/testutils/test-utils');

let PORTMANAGER;

class portManager {
  // //////////////////////////////////////////////////////////////////////////////
  // / @brief finds a free port
  // //////////////////////////////////////////////////////////////////////////////
  constructor(options) {
    this.usedPorts = [];
    this.minPort = options['minPort'];
    this.maxPort = options['maxPort'];
    if (typeof this.maxPort !== 'number') {
      this.maxPort = 32768;
    }

    if (this.maxPort - this.minPort < 0) {
      throw new Error('minPort ' + this.minPort + ' is smaller than maxPort ' + this.maxPort);
    }
  }
  deregister(port) {
    let deletePortIndex = this.usedPorts.indexOf(port);
    if (deletePortIndex > -1) {
      this.usedPorts.splice(deletePortIndex, 1);
    }
  }
  findFreePort() {
    let tries = 0;
    while (true) {
      const port = Math.floor(Math.random() * (this.maxPort - this.minPort)) + this.minPort;
      tries++;
      if (tries > 20) {
        throw new Error('Couldn\'t find a port after ' + tries + ' tries. portrange of ' + this.minPort + ', ' + this.maxPort + ' too narrow?');
      }
      if (this.usedPorts.indexOf(port) >= 0) {
        continue;
      }
      const free = internal.testPort('tcp://0.0.0.0:' + port);

      if (free) {
        this.usedPorts.push(port);
        return port;
      }

      internal.wait(0.1, false);
    }
  }
}

exports.registerOptions = function(optionsDefaults, optionsDocumentation) {
  tu.CopyIntoObject(optionsDefaults, {
    'minPort': 1024,
    'maxPort': 32768,
  });

  tu.CopyIntoList(optionsDocumentation, [
    ' SUT port range options:',
    '   - `minPort`: minimum port number to use',
    '   - `maxPort`: maximum port number to use',
    ''
  ]);
};

exports.getPortManager = function(options) {
  if (! PORTMANAGER) {
    PORTMANAGER = new portManager(options);
  }
  return PORTMANAGER;
};

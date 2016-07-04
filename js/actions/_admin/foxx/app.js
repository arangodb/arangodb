/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief foxx administration actions
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
// / @author Dr. Frank Celler
// / @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var actions = require('@arangodb/actions');
var foxxManager = require('@arangodb/foxx/manager');

var easyPostCallback = actions.easyPostCallback;

// //////////////////////////////////////////////////////////////////////////////
// / @brief sets up a Foxx application
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/foxx/setup',
  prefix: false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var mount = body.mount;

      return foxxManager.setup(mount);
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief tears down a Foxx application
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/foxx/teardown',
  prefix: false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var mount = body.mount;

      return foxxManager.teardown(mount);
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief installs a Foxx application
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/foxx/install',
  prefix: false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var appInfo = body.appInfo;
      var mount = body.mount;
      var options = body.options;
      return foxxManager.install(appInfo, mount, options);
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief uninstalls a Foxx application
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/foxx/uninstall',
  prefix: false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var mount = body.mount;
      var options = body.options || {};

      return foxxManager.uninstall(mount, options);
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief replaces a Foxx application
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/foxx/replace',
  prefix: false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var appInfo = body.appInfo;
      var mount = body.mount;
      var options = body.options;

      return foxxManager.replace(appInfo, mount, options);
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief upgrades a Foxx application
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/foxx/upgrade',
  prefix: false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var appInfo = body.appInfo;
      var mount = body.mount;
      var options = body.options;

      return foxxManager.upgrade(appInfo, mount, options);
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief configures a Foxx application
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/foxx/configure',
  prefix: false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var mount = body.mount;
      var options = body.options;
      if (options && options.configuration) {
        options = options.configuration;
      }

      return foxxManager.configure(mount, {configuration: options || {}});
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief Gets the configuration of a Foxx application
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/foxx/configuration',
  prefix: false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var mount = body.mount;

      return foxxManager.configuration(mount);
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief configures a Foxx application's dependencies
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/foxx/set-dependencies',
  prefix: false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var mount = body.mount;
      var options = body.options;

      return foxxManager.updateDeps(mount, {dependencies: options || {}});
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief Gets the dependencies of a Foxx application
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/foxx/dependencies',
  prefix: false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var mount = body.mount;

      return foxxManager.dependencies(mount);
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief Toggles the development mode of a foxx application
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/foxx/development',
  prefix: false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var mount = body.mount;
      var activate = body.activate;
      if (activate) {
        return foxxManager.development(mount);
      } else {
        return foxxManager.production(mount);
      }
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief Run tests for an app
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/foxx/tests',
  prefix: false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var mount = body.mount;
      var options = body.options;
      return foxxManager.runTests(mount, options);
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief Run script for an app
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/foxx/script',
  prefix: false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var name = body.name;
      var mount = body.mount;
      var options = body.options;
      return foxxManager.runScript(name, mount, options);
    }
  })
});

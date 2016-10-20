/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief foxx administration actions
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
// / @author Copyright 2014-2016, ArangoDB GmbH, Cologne, Germany
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var actions = require('@arangodb/actions');
var foxxManager = require('@arangodb/foxx/manager');

var easyPostCallback = actions.easyPostCallback;

// //////////////////////////////////////////////////////////////////////////////
// / @brief sets up a Foxx service
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
// / @brief tears down a Foxx service
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
// / @brief installs a Foxx service
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
      return foxxManager.install(appInfo, mount, options).simpleJSON();
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief uninstalls a Foxx service
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/foxx/uninstall',
  prefix: false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var mount = body.mount;
      var options = body.options || {};

      return foxxManager.uninstall(mount, options).simpleJSON();
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief replaces a Foxx service
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

      return foxxManager.replace(appInfo, mount, options).simpleJSON();
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief upgrades a Foxx service
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

      return foxxManager.upgrade(appInfo, mount, options).simpleJSON();
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief configures a Foxx service
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
      foxxManager.setConfiguration(mount, {configuration: options || {}});
      return foxxManager.lookupService(mount).simpleJSON();
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief Gets the configuration of a Foxx service
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
// / @brief configures a Foxx service's dependencies
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/foxx/set-dependencies',
  prefix: false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var mount = body.mount;
      var options = body.options;

      foxxManager.setDependencies(mount, {dependencies: options || {}});
      return foxxManager.lookupService(mount).simpleJSON();
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief Gets the dependencies of a Foxx service
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/foxx/dependencies',
  prefix: false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var mount = body.mount;

      const deps = foxxManager.dependencies(mount);
      for (const key of Object.keys(deps)) {
        const dep = deps[key];
        deps[key] = {
          definition: dep,
          title: dep.title,
          current: dep.current
        };
        delete dep.title;
        delete dep.current;
      }
      return deps;
    }
  })
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief Toggles the development mode of a foxx service
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
        return foxxManager.development(mount).simpleJSON();
      } else {
        return foxxManager.production(mount).simpleJSON();
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

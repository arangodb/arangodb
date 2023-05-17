'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// / Copyright 2012 triagens GmbH, Cologne, Germany
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
// //////////////////////////////////////////////////////////////////////////////

var internal = require('internal');
var common = require('@arangodb/common');

Object.keys(common).forEach(function (key) {
  exports[key] = common[key];
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief isServer
// //////////////////////////////////////////////////////////////////////////////

exports.isServer = false;

// //////////////////////////////////////////////////////////////////////////////
// / @brief isClient
// //////////////////////////////////////////////////////////////////////////////

exports.isClient = true;

// //////////////////////////////////////////////////////////////////////////////
// / @brief class "ArangoCollection"
// //////////////////////////////////////////////////////////////////////////////

// cannot yet not use arangodb
exports.ArangoCollection = require('@arangodb/arango-collection').ArangoCollection;

// //////////////////////////////////////////////////////////////////////////////
// / @brief class "ArangoConnection"
// //////////////////////////////////////////////////////////////////////////////

exports.ArangoConnection = internal.ArangoConnection;

// //////////////////////////////////////////////////////////////////////////////
// / @brief class "ArangoDatabase"
// //////////////////////////////////////////////////////////////////////////////

// cannot yet not use arangodb
exports.ArangoDatabase = require('@arangodb/arango-database').ArangoDatabase;

// //////////////////////////////////////////////////////////////////////////////
// / @brief class "ArangoStatement"
// //////////////////////////////////////////////////////////////////////////////

// cannot yet not use arangodb
exports.ArangoStatement = require('@arangodb/arango-statement').ArangoStatement;

// //////////////////////////////////////////////////////////////////////////////
// / @brief class "ArangoQueryCursor"
// //////////////////////////////////////////////////////////////////////////////

// cannot yet not use arangodb
exports.ArangoQueryCursor = require('@arangodb/arango-query-cursor').ArangoQueryCursor;

// //////////////////////////////////////////////////////////////////////////////
// / @brief class "ArangoView"
// //////////////////////////////////////////////////////////////////////////////

// cannot yet not use arangodb
exports.ArangoView = require('@arangodb/arango-view').ArangoView;

// //////////////////////////////////////////////////////////////////////////////
// / @brief the global "db" and "arango" object
// //////////////////////////////////////////////////////////////////////////////

if (typeof internal.arango !== 'undefined') {
  try {
    exports.arango = internal.arango;
    let db = new exports.ArangoDatabase(internal.arango);
    // proxy the db object, so we can track access to non-existing collections
    exports.db = new Proxy(db, {
      get(target, name) {
        if (!target.hasOwnProperty(name) && target[name] === undefined && typeof name === 'string') {
          // unknown collection, try re-populating the cache
          try {
            db._collections();
          } catch (err) {}
        }
        return target[name];
      },
    });
    internal.db = exports.db; // TODO remove
  } catch (err) {
    internal.print('cannot connect to server: ' + String(err));
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief the server version
// //////////////////////////////////////////////////////////////////////////////

exports.plainServerVersion = function () {
  if (internal.arango) {
    let version = internal.arango.getVersion();
    let devel = version.match(/(.*)\.devel/);

    if (devel !== null) {
      version = devel[1] + '.0';
    } else {
      devel = version.match(/(.*)((milestone|alpha|beta|devel|rc)[0-9]*)$/);

      if (devel !== null) {
        version = devel[1] + '0';
      }
    }

    return version;
  } else {
    return undefined;
  }
};

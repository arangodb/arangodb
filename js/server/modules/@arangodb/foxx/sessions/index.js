'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2015-2016 ArangoDB GmbH, Cologne, Germany
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
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const assert = require('assert');
const il = require('@arangodb/util').inline;
const cookieTransport = require('@arangodb/foxx/sessions/transports/cookie');
const headerTransport = require('@arangodb/foxx/sessions/transports/header');
const collectionStorage = require('@arangodb/foxx/sessions/storages/collection');

module.exports = function sessionMiddleware (cfg) {
  if (!cfg) {
    cfg = {};
  }
  let storage = cfg.storage;
  if (cfg.storages) {
    console.warn(il`
      Found unexpected "storages" option in session middleware.
      Did you mean "storage"?
    `);
  }
  assert(storage, 'No session storage specified');
  if (typeof storage === 'string' || storage.isArangoCollection) {
    storage = collectionStorage(storage);
  } else if (typeof storage === 'function') {
    storage = storage();
  }
  if (storage.toClient) {
    console.warn(il`
      Found unexpected "toClient" method on session storage.
      Did you mean "forClient"?
    `);
  }
  assert(
    storage.forClient && storage.fromClient,
    'Session storage must have a forClient and fromClient method'
  );
  if (cfg.transports) {
    console.warn(il`
      Found unexpected "transports" option in session middleware.
      Did you mean "transport"?
    `);
  }
  let transports = cfg.transport || [];
  if (!Array.isArray(transports)) {
    transports = [transports];
  }
  transports = transports.map((transport) => {
    if (transport === 'cookie') {
      return cookieTransport();
    }
    if (transport === 'header') {
      return headerTransport();
    }
    if (typeof transport === 'function') {
      transport = transport();
    }
    assert(
      transport.get || transport.set,
      'Session transport must have a get and/or set method'
    );
    return transport;
  });
  assert(transports.length > 0, 'Must specify at least one session transport');
  const autoCreate = cfg.autoCreate !== false;
  return {
    storage,
    transport: transports,
    register() {
      return (req, res, next) => {
        let sid = null;
        for (const transport of transports) {
          if (typeof transport.get === 'function') {
            sid = transport.get(req) || null;
          }
          if (sid) {
            break;
          }
        }
        let payload = sid && storage.fromClient(sid);
        if (!payload) {
          if (!autoCreate) {
            payload = null;
          } else if (typeof storage.new === 'function') {
            payload = storage.new();
          } else {
            payload = {};
          }
        }
        req.sessionStorage = storage;
        req.session = payload;
        next();
        if (req.session) {
          sid = storage.forClient(req.session);
          for (const transport of transports.reverse()) {
            if (typeof transport.set === 'function') {
              transport.set(res, sid);
            }
          }
        } else if (payload) {
          for (const transport of transports.reverse()) {
            if (typeof transport.clear === 'function') {
              transport.clear(req);
            }
          }
        }
      };
    }
  };
};

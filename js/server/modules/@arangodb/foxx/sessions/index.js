'use strict';
////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Sessions Middleware
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Alan Plum
/// @author Copyright 2015, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const cookieTransport = require('@arangodb/foxx/sessions/transports/cookie');
const headerTransport = require('@arangodb/foxx/sessions/transports/header');

module.exports = function sessionMiddleware(cfg) {
  if (!cfg) {
    cfg = {};
  }
  let storage = cfg.storage;
  if (!storage) {
    throw new Error('No session storage specified');
  }
  if (typeof storage === 'function') {
    storage = storage();
  }
  if (!storage.forClient || !storage.fromClient) {
    throw new Error('Session storage has no forClient/fromClient method');
  }
  let transports = cfg.transport || [];
  if (!Array.isArray(transports)) {
    transports = [transports];
  }
  transports = transports.map(function (transport) {
    if (transport === 'cookie') {
      return cookieTransport();
    }
    if (transport === 'header') {
      return headerTransport();
    }
    if (typeof transport === 'function') {
      transport = transport();
    }
    if (!transport.get && !transport.set) {
      throw new Error('Session handler has no get/set method');
    }
    return transport;
  });
  if (!transports.length) {
    throw new Error('No session transport spcified');
  }
  return {
    config: {
      storage: storage,
      transports: transports
    },
    register() {
      return function (req, res, next) {
        let sid = transports.reduce(function (sid, transport) {
          if (sid !== null) {
            return sid;
          }
          if (typeof transport.get === 'function') {
            sid = transport.get(req);
          }
          return sid || null;
        }, null);
        if (sid) {
          req.session = storage.fromClient(sid);
        } else if (typeof storage.new === 'function') {
          req.session = storage.new();
        } else {
          req.session = {};
        }
        next();
        sid = storage.forClient(req.session);
        transports.reverse().forEach(function (transport) {
          if (typeof transport.set === 'function') {
            transport.set(req, sid);
          }
        });
      };
    }
  };
};

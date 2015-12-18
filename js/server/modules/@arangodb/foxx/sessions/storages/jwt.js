'use strict';
////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Sessions JWT Storage
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

const crypto = require('@arangodb/crypto');

module.exports = function jwtStorage(cfg) {
  if (!cfg) {
    cfg = {};
  }
  const expiry = (cfg.expiry || 60) * 60 * 1000;
  return {
    fromClient(sid) {
      const session = crypto.jwtDecode(cfg.secret, sid, !cfg.verify);
      if (session.iat < Date.now() - expiry) {
        return session.payload;
      }
      return null;
    },
    forClient(payload) {
      return crypto.jwtEncode(cfg.secret, {
        payload: payload,
        iat: Date.now()
      }, cfg.algorithm);
    },
    new() {
      return {};
    }
  };
};

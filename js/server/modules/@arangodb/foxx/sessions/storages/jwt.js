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
const crypto = require('@arangodb/crypto');

module.exports = function jwtStorage (cfg) {
  if (typeof cfg === 'string') {
    cfg = {secret: cfg};
  }
  if (!cfg) {
    cfg = {};
  }
  assert(cfg.algorithm === 'none' || cfg.secret, `Must pass a JWT secret for "${cfg.algorithm}" algorithm`);
  assert(cfg.algorithm !== 'none' || !cfg.secret, 'Must NOT pass a JWT secret for "none" algorithm');
  const algorithm = cfg.algorithm || 'HS512';
  const ttl = (cfg.ttl || 60 * 60);
  const maxVal = cfg.maxExp || Infinity;
  return {
    fromClient (sid) {
      const token = crypto.jwtDecode(cfg.secret, sid, cfg.verify === false);
      if (Date.now() > token.exp * 1000 || token.exp > maxVal) {
        return null;
      }
      return {
        uid: token.uid,
        created: token.iat * 1000,
        data: token.payload
      };
    },
    forClient (session) {
      const token = {
        uid: session.uid,
        iat: Math.floor(session.created / 1000),
        payload: session.data,
        exp: Math.floor(Date.now() / 1000) + ttl
      };
      return crypto.jwtEncode(cfg.secret, token, algorithm);
    },
    new () {
      return {
        uid: null,
        created: Date.now(),
        data: null
      };
    }
  };
};

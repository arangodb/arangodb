'use strict';

// //////////////////////////////////////////////////////////////////////////////
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
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const crypto = require('@arangodb/crypto');
const Y2K = Date.UTC(2000, 0, 1);
const DAYS_PER_YEAR = 365.242199;
const MS_PER_YEAR = DAYS_PER_YEAR * 24 * 60 * 60 * 1000;

module.exports = function auth (cfg) {
  if (typeof cfg === 'string') {
    cfg = {method: cfg};
  }
  if (!cfg) {
    cfg = {};
  }
  const hashMethod = cfg.method || 'sha256';
  const saltLength = cfg.saltLength || 16;
  const workFactor = cfg.workFactor || 1;

  return {
    verify(authData, password = '') {
      if (typeof authData === 'string') {
        authData = {hash: authData};
      } else if (!authData) {
        authData = {};
      }
      const method = authData.method || hashMethod;
      const salt = authData.salt || '';
      const storedHash = authData.hash || '';
      const iter = authData.iter || 1;
      const generatedHash = (
        method === 'pbkdf2'
        ? crypto.pbkdf2(salt, password, iter, salt.length)
        : crypto[method](salt + password)
      );
      return crypto.constantEquals(storedHash, generatedHash);
    },

    create(password) {
      const salt = crypto.genRandomSalt(saltLength);
      const method = hashMethod;
      if (method === 'pbkdf2') {
        const years = (Date.now() - Y2K) / MS_PER_YEAR;
        const iter = Math.floor(1000 * Math.pow(2, years / 2) * workFactor);
        return {
          method,
          iter,
          salt,
          hash: crypto.pbkdf2(salt, password, iter, saltLength)
        };
      }
      const hash = crypto[method](salt + password);
      return {method, salt, hash};
    }
  };
};

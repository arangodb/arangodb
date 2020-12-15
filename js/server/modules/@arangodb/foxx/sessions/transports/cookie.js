'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2015 ArangoDB GmbH, Cologne, Germany
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

module.exports = function cookieTransport (cfg) {
  if (!cfg) {
    cfg = {};
  } else if (typeof cfg === 'string') {
    cfg = {name: cfg};
  }
  assert(!cfg.ttl || typeof cfg.ttl === 'number', 'TTL must be a number or not set');
  assert(!cfg.algorithm || cfg.secret, 'Must specify a secret when specifying an algorithm');
  const name = cfg.name || 'sid';
  const ttl = cfg.ttl || undefined;
  const opts = {};
  if (cfg.secret) {
    opts.secret = cfg.secret;
    opts.algorithm = cfg.algorithm;
  }
  if (cfg.path) {
    opts.path = cfg.path;
  }
  if (cfg.domain) {
    opts.domain = cfg.domain;
  }
  if (cfg.secure) {
    opts.secure = cfg.secure;
  }
  if (cfg.httpOnly) {
    opts.httpOnly = cfg.httpOnly;
  }
  return {
    get (req) {
      return req.cookie(name, opts);
    },
    set (res, value) {
      res.cookie(name, value, Object.assign({}, opts, {ttl}));
    },
    clear (res) {
      res.cookie(name, '', Object.assign({}, opts, {ttl: -1 * 60 * 60 * 24}));
    }
  };
};

'use strict';
////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Sessions Cookie Transport
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

module.exports = function cookieTransport(cfg) {
  if (!cfg) {
    cfg = 'sid';
  }
  if (typeof cfg === 'string') {
    cfg = {name: cfg};
  }
  const signed = cfg.secret ? {
    secret: cfg.secret,
    algorithm: cfg.algorithm
  } : undefined;
  return {
    get(req) {
      return req.cookie(cfg.name, signed && {signed: signed});
    },
    set(res, value) {
      res.cookie(cfg.name, value, {
        ttl: cfg.ttl,
        signed: signed
      });
    }
  };
};

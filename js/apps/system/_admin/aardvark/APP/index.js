'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Alan Plum
////////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const sessionsMiddleware = require('@arangodb/foxx/sessions');
const cookieTransport = require('@arangodb/foxx/sessions/transports/cookie');
const systemStorage = require('@arangodb/foxx/sessions/storages/_system');
const auth = require('@arangodb/foxx/auth');

module.context.sessions = systemStorage();
module.context.auth = auth('sha256');

module.context.use(sessionsMiddleware({
  transport: cookieTransport(`arango_sid_${db._name()}`),
  storage: module.context.sessions
}));

module.context.use((req, res, next) => {
  if (module.context.configuration.sessionPruneProb > Math.random()) {
    module.context.sessions.prune();
  }
  next();
});

module.context.use(require('./aardvark'));
module.context.use('/foxxes', require('./foxxes'));
module.context.use('/cluster', require('./cluster'));
module.context.use('/statistics', require('./statistics'));

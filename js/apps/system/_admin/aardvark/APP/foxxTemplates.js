'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Alan Plum
////////////////////////////////////////////////////////////////////////////////

const joi = require('joi');
const httperr = require('http-errors');
const internal = require('internal');
const FoxxManager = require('@arangodb/foxx/manager');
const createRouter = require('@arangodb/foxx/router');


const router = createRouter();
module.exports = router;

router.use((req, res, next) => {
  if (!internal.options()['server.disable-authentication'] && !req.session.uid) {
    throw new httperr.Unauthorized();
  }
  next();
});

router.get('/devMode', (req, res) => res.json(false));

router.post('/generate', (req, res) => res.json(
  FoxxManager.install('EMPTY', '/todo', req.body)
))
.body(joi.object({
  applicationContext: joi.string().optional(),
  path: joi.string().optional(),
  name: joi.string().required(),
  collectionNames: joi.array().required(),
  authenticated: joi.boolean().required(),
  author: joi.string().required(),
  description: joi.string().required(),
  license: joi.string().required()
}), 'The configuration for the template.');

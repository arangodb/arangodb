'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2013 triAGENS GmbH, Cologne, Germany
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
const dd = require('dedent');
const internal = require('internal');
const db = require('@arangodb').db;
const errors = require('@arangodb').errors;
const joinPath = require('path').posix.join;
const notifications = require('@arangodb/configuration').notifications;
const examples = require('@arangodb/graph-examples/example-graph');
const systemStorage = require('@arangodb/foxx/sessions/storages/_system');
const createRouter = require('@arangodb/foxx/router');
const users = require('@arangodb/users');

const ERROR_USER_NOT_FOUND = errors.ERROR_USER_NOT_FOUND.code;
const API_DOCS = require(module.context.fileName('api-docs.json'));
API_DOCS.basePath = `/_db/${encodeURIComponent(db._name())}`;

const sessions = systemStorage();
const router = createRouter();
module.exports = router;

let isTrustedProxy = function(proxyAddress) {
  return true;
}

router.get('/config.js', function(req, res) {
  let basePath = '';
  if (req.headers.hasOwnProperty('x-forwarded-for')
      && req.headers.hasOwnProperty('x-script-name')
      && isTrustedProxy("xxx")) {
    basePath = req.headers['x-script-name'];
  }
  res.set('content-type', 'text/javascript');
  res.send("var frontendConfig = " + JSON.stringify({"basePath": basePath}) + ";");
});

router.get('/whoAmI', function(req, res) {
  res.json({user: req.session.uid || null});
})
.summary('Return the current user')
.description(dd`
  Returns the current user or "null" if the user is not authenticated.
  Returns "false" if authentication is disabled.
`);


router.post('/logout', function (req, res) {
  sessions.clear(req.session);
  delete req.session;
  res.json({success: true});
})
.summary('Log out')
.description(dd`
  Destroys the current session and revokes any authentication.
`);


router.post('/login', function (req, res) {
  const currentDb = db._name();
  /*
  const actualDb = req.body.database;
  if (actualDb !== currentDb) {
    res.redirect(307, joinPath(
      '/_db',
      encodeURIComponent(actualDb),
      module.context.mount,
      '/login'
    ));
    return;
  }
  */
  const user = req.body.username;
  const valid = users.isValid(user, req.body.password);

  if (!valid) {
    res.throw('unauthorized', 'Bad username or password');
  }

  sessions.setUser(req.session, user);
  sessions.save(req.session);

  res.json({user});
})
.body({
  username: joi.string().required(),
  password: joi.string().required().allow('')
  //database: joi.string().default(db._name())
}, 'Login credentials.')
.error('unauthorized', 'Invalid credentials.')
.summary('Log in')
.description(dd`
  Authenticates the user for the active session with a username and password.
  Creates a new session if none exists.
`);


const authRouter = createRouter();
router.use(authRouter);


authRouter.use((req, res, next) => {
  if (!req.session.uid) {
    res.throw('unauthorized');
  }
  next();
});


authRouter.get('/api/*', module.context.apiDocumentation({
  swaggerJson(req, res) {
    res.json(API_DOCS);
  }
}))
.summary('System API documentation')
.description(dd`
  Mounts the system API documentation.
`);


authRouter.get('shouldCheckVersion', function(req, res) {
  const versions = notifications.versions();
  res.json(Boolean(versions && versions.enableVersionNotification));
})
.summary('Is version check allowed')
.description(dd`
  Check if version check is allowed.
`);


authRouter.post('disableVersionCheck', function(req, res) {
  notifications.setVersions({enableVersionNotification: false});
  res.json('ok');
})
.summary('Disable version check')
.description(dd`
  Disable the version check in web interface
`);


authRouter.post('/query/explain', function(req, res) {
  const bindVars = req.body.bindVars;
  const query = req.body.query;
  const id = req.body.id;
  const batchSize = req.body.batchSize;
  let msg = null;

  try {
    if (bindVars) {
      msg = require('@arangodb/aql/explainer').explain({
        query: query,
        bindVars: bindVars,
        batchSize: batchSize,
        id: id
      }, {colors: false}, false, bindVars);
    } else {
      msg = require('@arangodb/aql/explainer').explain(query, {colors: false}, false);
    }
  } catch (e) {
    res.throw('bad request', e.message, {cause: e});
  }

  res.json({msg});
})
.body(joi.object({
  query: joi.string().required(),
  bindVars: joi.object().optional(),
  batchSize: joi.number().optional(),
  id: joi.string().optional()
}).required(), 'Query and bindVars to explain.')
.summary('Explains a query')
.description(dd`
  Explains a query in a more user-friendly way than the query_api/explain
`);


authRouter.post('/query/upload/:user', function(req, res) {
  let user;

  try {
    user = users.document(req.session.uid);
  } catch (e) {
    if (!e.isArangoError || e.errorNum !== ERROR_USER_NOT_FOUND) {
      throw e;
    }
    sessions.setUser(req.session);
    res.throw('not found');
  }

  if (!user.extra.queries) {
    user.extra.queries = [];
  }

  const existingQueries = user.extra.queries
  .map(query => query.name);

  for (const query of req.body) {
    if (existingQueries.indexOf(query.name) !== -1) {
      existingQueries.push(query.name);
      user.extra.queries.push(query);
    }
  }

  users.update(user.user, undefined, undefined, user.extra);
  res.json(user.extra.queries);
})
.pathParam('user', joi.string().required(), 'Username. Ignored if authentication is enabled.')
.body(joi.array().items(joi.object({
  name: joi.string().required(),
  parameter: joi.any().optional(),
  value: joi.any().optional()
}).required()).required(), 'User query array to import.')
.error('not found', 'User does not exist.')
.summary('Upload user queries')
.description(dd`
  This function uploads all given user queries.
`);


authRouter.get('/query/download/:user', function(req, res) {
  let user;

  try {
    user = users.document(req.session.uid);
  } catch (e) {
    if (!e.isArangoError || e.errorNum !== ERROR_USER_NOT_FOUND) {
      throw e;
    }
    sessions.setUser(req.session);
    res.throw('not found');
  }

  res.attachment(`queries-${db._name()}-${user.user}.json`);
  res.json(user.extra.queries || []);
})
.pathParam('user', joi.string().required(), 'Username. Ignored if authentication is enabled.')
.error('not found', 'User does not exist.')
.summary('Download stored queries')
.description(dd`
  Download and export all queries from the given username.
`);


authRouter.get('/query/result/download/:query', function(req, res) {
  let query;
  try {
    query = internal.base64Decode(req.pathParams.query);
    query = JSON.parse(query);
  }
  catch (e) {
    res.throw('bad request', e.message, {cause: e});
  }

  const result = db._query(query.query, query.bindVars).toArray();
  res.attachment(`results-${db._name()}.json`);
  res.json(result);
})
.pathParam('query', joi.string().required(), 'Base64 encoded query.')
.error('bad request', 'The query is invalid or malformed.')
.summary('Download the result of a query')
.description(dd`
  This function downloads the result of a user query.
`);


authRouter.post('/graph-examples/create/:name', function(req, res) {
  const name = req.pathParams.name;

  if (['knows_graph', 'social', 'routeplanner'].indexOf(name) === -1) {
    res.throw('not found');
  }

  const g = examples.loadGraph(name);
  res.json({error: !g});
})
.pathParam('name', joi.string().required(), 'Name of the example graph.')
.summary('Create example graphs')
.description(dd`
  Create one of the given example graphs.
`);


authRouter.post('/job', function(req, res) {
  db._frontend.save(Object.assign(req.body, {model: 'job'}));
  res.json(true);
})
.body(joi.object({
  id: joi.any().required(),
  collection: joi.any().required(),
  type: joi.any().required(),
  desc: joi.any().required()
}).required())
.summary('Store job id of a running job')
.description(dd`
  Create a new job id entry in a specific system database with a given id.
`);


authRouter.delete('/job', function(req, res) {
  db._frontend.removeByExample({model: 'job'}, false);
  res.json(true);
})
.summary('Delete all jobs')
.description(dd`
  Delete all jobs in a specific system database with a given id.
`);


authRouter.delete('/job/:id', function(req, res) {
  db._frontend.removeByExample({id: req.pathParams.id}, false);
  res.json(true);
})
.summary('Delete a job id')
.description(dd`
  Delete an existing job id entry in a specific system database with a given id.
`);


authRouter.get('/job', function(req, res) {
  const result = db._frontend.all().toArray();
  res.json(result);
})
.summary('Return all job ids.')
.description(dd`
  This function returns the job ids of all currently running jobs.
`);

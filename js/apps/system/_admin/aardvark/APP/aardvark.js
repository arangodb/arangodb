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
const notifications = require('@arangodb/configuration').notifications;
const examples = require('@arangodb/graph-examples/example-graph');
const createRouter = require('@arangodb/foxx/router');

const API_DOCS = require(module.context.fileName('api-docs.json'));
API_DOCS.basePath = `/_db/${encodeURIComponent(db._name())}`;

const sessions = module.context.sessions;
const auth = module.context.auth;
const router = createRouter();
module.exports = router;


router.get('/whoAmI', function(req, res) {
  let user = null;
  if (internal.options()['server.disable-authentication']) {
    user = false;
  } else if (req.session && req.session.uid) {
    try {
      const doc = db._users.document(req.session.uid);
      user = doc.user;
    } catch (e) {
      if (!e.isArangoError || e.errorNum !== errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
        throw e;
      }
      sessions.setUser(req.session, null);
    }
  }
  res.json({user});
})
.summary('Return the current user')
.description(dd`
  Returns the current user or "null" if the user is not authenticated.
  Returns "false" if authentication is disabled.
`);


router.post('/logout', function (req, res) {
  if (req.session) {
    sessions.clear(req.session._key);
    delete req.session;
  }
  res.json({success: true});
})
.summary('Log out')
.description(dd`
  Destroys the current session and revokes any authentication.
`);


router.post('/login', function (req, res) {
  if (!req.session) {
    req.session = sessions.new();
  }

  const doc = db._users.firstExample({user: req.body.username});
  const valid = auth.verify(
    doc ? doc.authData.simple : null,
    req.body.password
  );

  if (!valid) {
    res.throw('unauthorized');
  }

  sessions.setUser(req.session, doc);
  const user = doc.user;
  res.json({user});
})
.body({
  username: joi.string().required(),
  password: joi.string().required().allow('')
}, 'Login credentials.')
.error('unauthorized', 'Invalid credentials.')
.summary('Log in')
.description(dd`
  Authenticates the user for the active session with a username and password.
  Creates a new session if none exists.
`);


router.get('/unauthorized', function(req, res) {
  res.throw('unauthorized');
})
.error('unauthorized')
.summary('Unauthorized')
.description(dd`
  Responds with a HTTP 401 response.
`);


const authRouter = createRouter();
router.use(authRouter);


authRouter.use((req, res, next) => {
  if (!internal.options()['server.disable-authentication'] && (!req.session || !req.session.uid)) {
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
  let msg = null;

  try {
    if (bindVars) {
      msg = require('@arangodb/aql/explainer').explain({
        query: query,
        bindVars: bindVars
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
  bindVars: joi.object().optional()
}).required(), 'Query and bindVars to explain.')
.summary('Explains a query')
.description(dd`
  Explains a query in a more user-friendly way than the query_api/explain
`);


authRouter.post('/query/upload/:user', function(req, res) {
  let doc;

  if (req.session && req.session.uid) {
    try {
      doc = db._users.document(req.session.uid);
    } catch (e) {
      if (!e.isArangoError || e.errorNum !== errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
        throw e;
      }
    }
  } else {
    doc = db._users.firstExample({user: req.pathParams.user});
  }

  if (!doc) {
    res.throw('not found');
  }

  if (!doc.userData.queries) {
    doc.userData.queries = [];
  }

  const existingQueries = doc.userData.queries
  .map(query => query.name);

  for (const query of req.body) {
    if (existingQueries.indexOf(query.name) !== -1) {
      existingQueries.push(query.name);
      doc.userData.queries.push(query);
    }
  }

  const meta = db._users.replace(doc, doc);
  res.json(meta);
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
  let doc;

  if (req.session && req.session.uid) {
    try {
      doc = db._users.document(req.session.uid);
    } catch (e) {
      if (!e.isArangoError || e.errorNum !== errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
        throw e;
      }
    }
  } else {
    doc = db._users.firstExample({user: req.pathParams.user});
  }

  if (!doc) {
    res.throw('not found');
  }

  res.attachment(`queries-${db._name()}-${doc.user}.json`);
  res.json(doc.userData.queries || []);
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
  db._frontend.removeByExample({model: 'job'}, true);
  res.json(true);
})
.summary('Delete all jobs')
.description(dd`
  Delete all jobs in a specific system database with a given id.
`);


authRouter.delete('/job/:id', function(req, res) {
  db._frontend.removeByExample({id: req.pathParams.id}, true);
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

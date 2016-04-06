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

const underscore = require('lodash');
const cluster = require('@arangodb/cluster');
const joi = require('joi');
const httperr = require('http-errors');
const contentDisposition = require('content-disposition');
const internal = require('internal');
const db = require('@arangodb').db;
const notifications = require('@arangodb/configuration').notifications;
const createRouter = require('@arangodb/foxx/router');
const NOT_FOUND = require('@arangodb').errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code;

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
      if (!e.isArangoError || e.errorNum !== NOT_FOUND) {
        throw e;
      }
      req.session.uid = null;
    }
  }
  res.json({user});
});

router.post('/logout', function (req, res) {
  req.session.uid = null;
  res.json({success: true});
});

router.post('/login', function (req, res) {
  if (!req.session) {
    req.session = module.context.sessions.new();
  }

  const doc = db._users.firstExample({user: req.body.username});
  const valid = module.context.auth.verify(
    doc ? doc.authData.simple : null,
    req.body.password
  );

  if (!valid) {
    throw new httperr.Unauthorized();
  }

  const user = doc.user;
  req.session.uid = doc._key;
  res.json({user});
})
.body({
  username: joi.string().required(),
  password: joi.string().required().allow('')
}, 'Login credentials.');

router.get('/unauthorized', function() {
  throw new httperr.Unauthorized();
});

router.get('/index.html', function(req, res) {
  res.redirect(req.makeAbsolute('standalone.html'));
});

const authRouter = createRouter();
router.use(authRouter);

authRouter.use((req, res, next) => {
  if (!internal.options()['server.disable-authentication'] && !req.user) {
    throw new httperr.Unauthorized();
  }
  next();
});

authRouter.get('/api/*', module.context.createSwaggerHandler(
  (req) => ({
    appPath: req.queryParams.mount,
    swaggerJson(req, res) {
      res.sendFile(module.context.fileName('api-docs.json'), {lastModified: true});
    }
  })
));

authRouter.get('shouldCheckVersion', function(req, res) {
  var versions = notifications.versions();
  res.json(Boolean(versions && versions.enableVersionNotification));
})
.summary('Is version check allowed')
.description('Check if version check is allowed.');

authRouter.post('disableVersionCheck', function(req, res) {
  notifications.setVersions({
    enableVersionNotification: false
  });
  res.json('ok');
})
.summary('Disable version check')
.description('Disable the version check in web interface');

authRouter.post('/query/explain', function(req, res) {

  var explain, query = req.body().query,
  bindVars = req.body().bindVars;

  if (query.length > 0) {
    try {
      if (bindVars) {
        explain = require('@arangodb/aql/explainer').explain({
          query: query,
          bindVars: bindVars
        }, {colors: false}, false, bindVars);
      }
      else {
        explain = require('@arangodb/aql/explainer').explain(query, {colors: false}, false);
      }
    }
    catch (e) {
      explain = JSON.stringify(e);
    }
  }


  res.json({msg: explain});

})
.summary('Explains a query')
.description('Explains a query in a more user-friendly way than the query_api/explain');


authRouter.post('/query/upload/:user', function(req, res) {
  var user = req.params('user');
  var queries, doc, queriesToSave;

  queries = req.body();
  doc = db._users.byExample({'user': user}).toArray()[0];
  queriesToSave = doc.userData.queries || [ ];

  underscore.each(queries, function(newq) {
    var found = false, i;
    for (i = 0; i < queriesToSave.length; ++i) {
      if (queriesToSave[i].name === newq.name) {
        queriesToSave[i] = newq;
        found = true;
        break;
      }
    }
    if (!found) {
      queriesToSave.push(newq);
    }
  });

  var toUpdate = {
    userData: {
      queries: queriesToSave
    }
  };

  var result = db._users.update(doc, toUpdate, true);
  res.json(result);
})
.summary('Upload user queries')
.description('This function uploads all given user queries');

authRouter.get('/query/download/:user', function(req, res) {
  var user = req.params('user');
  var result = db._users.byExample({'user': user}).toArray()[0];

  res.set('Content-Type', 'application/json');
  res.set('Content-Disposition', contentDisposition('queries.json'));

  if (result === null || result === undefined) {
    res.json([]);
  }
  else {
    res.json(result.userData.queries || []);
  }

})
.summary('Download stored queries')
.description('Download and export all queries from the given username.');

authRouter.get('/query/result/download/:query', function(req, res) {
  var query = req.params('query'),
  parsedQuery;

  var internal = require('internal');
  query = internal.base64Decode(query);
  try {
    parsedQuery = JSON.parse(query);
  }
  catch (ignore) {
  }

  var result = db._query(parsedQuery.query, parsedQuery.bindVars).toArray();
  res.set('Content-Type', 'application/json');
  res.set('Content-Disposition', contentDisposition('results.json'));
  res.json(result);

})
.summary('Download the result of a query')
.description('This function downloads the result of a user query.');

authRouter.post('/graph-examples/create/:name', function(req, res) {
  var name = req.params('name'), g,
  examples = require('@arangodb/graph-examples/example-graph.js');

  if (name === 'knows_graph') {
    g = examples.loadGraph('knows_graph');
  }
  else if (name === 'social') {
    g = examples.loadGraph('social');
  }
  else if (name === 'routeplanner') {
    g = examples.loadGraph('routeplanner');
  }

  if (typeof g === 'object') {
    res.json({error: false});
  }
  else {
    res.json({error: true});
  }

})
.summary('Create sample graphs')
.description('Create one of the given sample graphs.');

authRouter.post('/job', function(req, res) {

  if (req.body().id && req.body().collection && req.body().type && req.body().desc) {

    //store id in _system
    db._frontend.save({
      id: req.body().id,
      collection: req.body().collection, 
      type: req.body().type,
      model: 'job',
      desc: req.body().desc
    });

    res.json(true);
  }
  else {
    res.json(false);
  }

})
.summary('Store job id of a running job')
.description('Create a new job id entry in a specific system database with a given id.');

authRouter.delete('/job/', function(req, res) {
  db._frontend.removeByExample({
    model: 'job'
  }, true);
  res.json(true);
})
.summary('Delete all jobs')
.description('Delete all jobs in a specific system database with a given id.');

authRouter.delete('/job/:id', function(req, res) {

  var id = req.params('id');

  if (id) {
    db._frontend.removeByExample({
      id: id
    }, true);
    res.json(true);
  }
  else {
    res.json(false);
  }

})
.summary('Delete a job id')
.description('Delete an existing job id entry in a specific system database with a given id.');

authRouter.get('/job', function(req, res) {

  var result = db._frontend.all().toArray();
  res.json(result);

})
.summary('Return all job ids.')
.description('This function returns the job ids of all currently running jobs.');

/*global require, applicationContext */
(function () {
  'use strict';
  var _ = require('underscore'),
    joi = require('joi'),
    Foxx = require('org/arangodb/foxx'),
    errors = require('./errors'),
    controller = new Foxx.Controller(applicationContext),
    api = Foxx.requireApp(applicationContext.mount).sessionStorage,
    sessionId = joi.string().description('Session ID');

  controller.post('/', function (req, res) {
    var session = api.create(req.body());
    res.status(201);
    res.json(session.forClient());
  })
    .errorResponse(SyntaxError, 400, 'Malformed or non-JSON session data.')
    .summary('Create session')
    .notes('Stores the given sessionData in a new session.');

  controller.get('/:sid', function (req, res) {
    var session = api.get(req.urlParameters.sid);
    res.json(session.forClient());
  })
    .pathParam('sid', {type: sessionId})
    .errorResponse(errors.SessionExpired, 404, 'Session has expired')
    .errorResponse(errors.SessionNotFound, 404, 'Session does not exist')
    .summary('Read session')
    .notes('Fetches the session with the given sid.');

  controller.put('/:sid', function (req, res) {
    var body = JSON.parse(req.rawBody()),
      session = api.get(req.urlParameters.sid);
    session.set('sessionData', body);
    session.save();
    res.json(session.forClient());
  })
    .pathParam('sid', {type: sessionId})
    .errorResponse(errors.SessionExpired, 404, 'Session has expired')
    .errorResponse(errors.SessionNotFound, 404, 'Session does not exist')
    .errorResponse(SyntaxError, 400, 'Malformed or non-JSON session data.')
    .summary('Update session (replace)')
    .notes('Updates the session with the given sid by replacing the sessionData.');

  controller.patch('/:sid', function (req, res) {
    var body = JSON.parse(req.rawBody()),
      session = api.get(req.urlParameters.sid);
    _.extend(session.get('sessionData'), body);
    session.save();
    res.json(session.forClient());
  })
    .pathParam('sid', {type: sessionId})
    .errorResponse(errors.SessionExpired, 404, 'Session has expired')
    .errorResponse(errors.SessionNotFound, 404, 'Session does not exist')
    .errorResponse(SyntaxError, 400, 'Malformed or non-JSON session data.')
    .summary('Update session')
    .notes('Updates the session with the given sid by merging its sessionData.');

  controller.delete('/:sid', function (req, res) {
    api.destroy(req.urlParameters.sid);
    res.status(204);
  })
    .pathParam('sid', {type: sessionId})
    .errorResponse(errors.SessionNotFound, 404, 'Session does not exist')
    .summary('Delete session')
    .notes('Removes the session with the given sid from the database.');
}());
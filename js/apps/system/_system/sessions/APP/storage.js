/*global applicationContext */
'use strict';

var _ = require('underscore');
var joi = require('joi');
var internal = require('internal');
var arangodb = require('org/arangodb');
var db = arangodb.db;
var Foxx = require('org/arangodb/foxx');
var errors = require('./errors');
var cfg = applicationContext.configuration;

var Session = Foxx.Model.extend({
  schema: {
    _key: joi.string().required(),
    uid: joi.string().optional(),
    sessionData: joi.object().required(),
    userData: joi.object().required(),
    created: joi.number().integer().required(),
    lastAccess: joi.number().integer().required(),
    lastUpdate: joi.number().integer().required()
  }
});

var sessions = new Foxx.Repository(
  db._collection('_sessions'),
  {model: Session}
);

function generateSessionId() {
  var sid = '';
  if (cfg.sidTimestamp) {
    sid = internal.base64Encode(Number(new Date()));
    if (cfg.sidLength === 0) {
      return sid;
    }
    sid += '-';
  }
  return sid + internal.genRandomAlphaNumbers(cfg.sidLength || 10);
}

function createSession(sessionData) {
  var sid = generateSessionId(cfg);
  var now = Number(new Date());
  var session = new Session({
    _key: sid,
    uid: null,
    sessionData: sessionData || {},
    userData: {},
    created: now,
    lastAccess: now,
    lastUpdate: now
  });
  sessions.save(session);
  return session;
}

function getSession(sid) {
  var session;
  db._executeTransaction({
    collections: {
      read: [sessions.collection.name()],
      write: [sessions.collection.name()]
    },
    action: function () {
      try {
        session = sessions.byId(sid);
      } catch (err) {
        if (
          err instanceof arangodb.ArangoError
            && err.errorNum === arangodb.ERROR_ARANGO_DOCUMENT_NOT_FOUND
        ) {
          throw new errors.SessionNotFound(sid);
        } else {
          throw err;
        }
      }

      var accessTime = internal.accessSid(sid);
      if (session.get('lastAccess') < accessTime) {
        session.set('lastAccess', accessTime);
      }
      session.enforceTimeout();

      var now = Number(new Date());
      sessions.collection.update(session.forDB(), {
        lastAccess: now
      });
      session.set('lastAccess', now);
      session.save();
    }
  });
  return session;
}

function deleteSession(sid) {
  try {
    sessions.removeById(sid);
  } catch (err) {
    if (
      err instanceof arangodb.ArangoError
        && err.errorNum === arangodb.ERROR_ARANGO_DOCUMENT_NOT_FOUND
    ) {
      throw new errors.SessionNotFound(sid);
    } else {
      throw err;
    }
  }
  return null;
}

_.extend(Session.prototype, {
  enforceTimeout: function () {
    if (this.hasExpired()) {
      throw new errors.SessionExpired(this.get('_key'));
    }
  },
  hasExpired: function () {
    return this.getTTL() === 0;
  },
  getTTL: function () {
    return Math.max(0, this.getExpiry() - Date.now());
  },
  getExpiry: function () {
    return this.get('lastAccess') + (Number(internal.options()['server.session-timeout']) * 1000);
  },
  setUser: function (user) {
    var session = this;
    if (user) {
      session.set('uid', user.get('_id'));
      session.set('userData', user.get('userData'));
      internal.createSid(session.get('_key'), user.get('user'));
    } else {
      delete session.attributes.uid;
      session.set('userData', {});
      internal.clearSid(session.get('_key'));
    }
    return session;
  },
  save: function () {
    var session = this;
    var now = Number(new Date());
    session.set('lastAccess', now);
    session.set('lastUpdate', now);
    sessions.replace(session);
    return session;
  },
  delete: function () {
    var session = this;
    var now = Number(new Date());
    session.set('lastAccess', now);
    session.set('lastUpdate', now);
    try {
      var key = session.get('_key');
      internal.clearSid(key);
      deleteSession(key);
      return true;
    } catch (e) {
      if (e instanceof errors.SessionNotFound) {
        return false;
      }
      throw e;
    }
  }
});

exports.create = createSession;
exports.get = getSession;
exports.delete = deleteSession;
exports.errors = errors;
exports.repository = sessions;
exports._generateSessionId = generateSessionId;

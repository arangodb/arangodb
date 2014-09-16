/*global require, exports, applicationContext */
(function () {
  'use strict';
  var _ = require('underscore'),
    joi = require('joi'),
    internal = require('internal'),
    arangodb = require('org/arangodb'),
    db = arangodb.db,
    Foxx = require('org/arangodb/foxx'),
    errors = require('./errors'),
    cfg = applicationContext.configuration,
    Session = Foxx.Model.extend({
      schema: {
        _key: joi.string().required(),
        uid: joi.string().optional(),
        sessionData: joi.object().required(),
        userData: joi.object().required(),
        created: joi.number().integer().required(),
        lastAccess: joi.number().integer().required(),
        lastUpdate: joi.number().integer().required()
      }
    }),
    sessions;

  if (applicationContext.mount.indexOf('/_system/') === 0) {
    sessions = new Foxx.Repository(
      db._collection('_sessions'),
      {model: Session}
    );
  } else {
    sessions = new Foxx.Repository(
      applicationContext.collection('sessions'),
      {model: Session}
    );
  }

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
    var sid = generateSessionId(cfg),
      now = Number(new Date()),
      session = new Session({
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
          session.enforceTimeout();
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
        var now = Number(new Date());
        sessions.collection.update(session.forDB(), {
          lastAccess: now
        });
        session.set('lastAccess', now);
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
      if (!cfg.timeToLive) {
        return Infinity;
      }
      return Math.max(0, this.getExpiry() - Date.now());
    },
    getExpiry: function () {
      if (!cfg.timeToLive) {
        return Infinity;
      }
      var prop = cfg.ttlType;
      if (!prop || !this.get(prop)) {
        prop = 'created';
      }
      return this.get(prop) + cfg.timeToLive;
    },
    setUser: function (user) {
      var session = this;
      if (user) {
        session.set('uid', user.get('_id'));
        session.set('userData', user.get('userData'));
      } else {
        delete session.attributes.uid;
        session.set('userData', {});
      }
      return session;
    },
    save: function () {
      var session = this,
        now = Number(new Date());
      session.set('lastAccess', now);
      session.set('lastUpdate', now);
      sessions.replace(session);
      return session;
    },
    delete: function () {
      var session = this,
        now = Number(new Date());
      session.set('lastAccess', now);
      session.set('lastUpdate', now);
      try {
        deleteSession(session.get('_key'));
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
}());
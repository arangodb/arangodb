/*jslint indent: 2, nomen: true, maxlen: 120, es5: true */
/*global require, exports, applicationContext */
(function () {
  'use strict';
  var _ = require('underscore'),
    joi = require('joi'),
    internal = require('internal'),
    arangodb = require('org/arangodb'),
    db = arangodb.db,
    addCookie = require('org/arangodb/actions').addCookie,
    crypto = require('org/arangodb/crypto'),
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
    sessions = new Foxx.Repository(
      applicationContext.collection('sessions'),
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
    var sid = generateSessionId(cfg),
      now = Number(new Date()),
      session = new Session({
        _key: sid,
        sid: sid,
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

  function fromCookie(req, cookieName, secret) {
    var session = null,
      value = req.cookies[cookieName],
      signature;
    if (value) {
      if (secret) {
        signature = req.cookies[cookieName + '_sig'] || '';
        if (!crypto.constantEquals(signature, crypto.hmac(secret, value))) {
          return null;
        }
      }
      try {
        session = getSession(value);
      } catch (e) {
        if (!(e instanceof errors.SessionNotFound)) {
          throw e;
        }
      }
    }
    return session;
  }

  _.extend(Session.prototype, {
    enforceTimeout: function () {
      if (!cfg.timeToLive) {
        return;
      }
      var now = Number(new Date()),
        prop = cfg.ttlType;
      if (!prop || !this.get(prop)) {
        prop = 'created';
      }
      if (cfg.timeToLive < (now - this.get(prop))) {
        throw new errors.SessionExpired(this.get('_key'));
      }
    },
    addCookie: function (res, cookieName, secret) {
      var value = this.get('_key'),
        ttl = cfg.timeToLive;
      ttl = ttl ? Math.floor(ttl / 1000) : undefined;
      addCookie(res, cookieName, value, ttl);
      if (secret) {
        addCookie(res, cookieName + '_sig', crypto.hmac(secret, value), ttl);
      }
    },
    clearCookie: function (res, cookieName, secret) {
      addCookie(res, cookieName, '', -(7 * 24 * 60 * 60));
      if (secret) {
        addCookie(res, cookieName + '_sig', '', -(7 * 24 * 60 * 60));
      }
    },
    setUser: function (user) {
      var session = this;
      if (user) {
        session.set('uid', user.get('_key'));
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

  exports.fromCookie = fromCookie;
  exports.create = createSession;
  exports.get = getSession;
  exports.delete = deleteSession;
  exports.errors = errors;
  exports.repository = sessions;
  exports._generateSessionId = generateSessionId;
}());
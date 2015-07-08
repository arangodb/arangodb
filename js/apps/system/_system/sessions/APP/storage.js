'use strict';
const _ = require('underscore');
const joi = require('joi');
const internal = require('internal');
const arangodb = require('org/arangodb');
const db = arangodb.db;
const Foxx = require('org/arangodb/foxx');
const errors = require('./errors');

const Session = Foxx.Model.extend({
  schema: {
    _key: joi.string().required(),
    uid: joi.string().allow(null).required().default(null),
    sessionData: joi.object().required().default('Empty object', Object),
    userData: joi.object().required().default('Empty object', Object),
    created: joi.number().integer().required().default('Current date', Date.now),
    lastAccess: joi.number().integer().required('Current date', Date.now),
    lastUpdate: joi.number().integer().required('Current date', Date.now)
  }
});

const sessions = new Foxx.Repository(
  db._collection('_sessions'),
  {model: Session}
);

function generateSessionId() {
  return internal.genRandomAlphaNumbers(20);
}

function createSession(sessionData, userData) {
  const sid = generateSessionId();
  let session = new Session({
    _key: sid,
    uid: (userData && userData._id) || null,
    sessionData: sessionData || {},
    userData: userData || {}
  });
  sessions.save(session);
  return session;
}

function deleteSession(sid) {
  try {
    sessions.removeById(sid);
  } catch (e) {
    if (
      e instanceof arangodb.ArangoError
      && e.errorNum === arangodb.ERROR_ARANGO_DOCUMENT_NOT_FOUND
    ) {
      throw new errors.SessionNotFound(sid);
    } else {
      throw e;
    }
  }
  return null;
}

Session.fromClient = function (sid) {
  let session;
  db._executeTransaction({
    collections: {
      read: [sessions.collection.name()],
      write: [sessions.collection.name()]
    },
    action() {
      try {
        session = sessions.byId(sid);

        const now = internal.accessSid(sid);
        session.set('lastAccess', now);
        session.enforceTimeout();

        sessions.collection.update(
          session.get('_key'),
          {lastAccess: now}
        );
      } catch (e) {
        if (
          e instanceof arangodb.ArangoError
          && e.errorNum === arangodb.ERROR_ARANGO_DOCUMENT_NOT_FOUND
        ) {
          throw new errors.SessionNotFound(sid);
        } else {
          throw e;
        }
      }
    }
  });
  return session;
};

_.extend(Session.prototype, {
  forClient() {
    return this.get('_key');
  },
  enforceTimeout() {
    if (this.hasExpired()) {
      throw new errors.SessionExpired(this.get('_key'));
    }
  },
  hasExpired() {
    return this.getTTL() === 0;
  },
  getTTL() {
    return Math.max(0, this.getExpiry() - Date.now());
  },
  getExpiry() {
    const timeout = Number(internal.options()['server.session-timeout']) * 1000;
    return this.get('lastAccess') + timeout;
  },
  setUser(user) {
    if (user) {
      this.set('uid', user.get('_id'));
      this.set('userData', user.get('userData'));
      internal.createSid(this.get('_key'), user.get('user'));
    } else {
      delete this.attributes.uid;
      this.set('userData', {});
      internal.clearSid(this.get('_key'));
    }
    return this;
  },
  save() {
    const now = Date.now();
    const key = this.get('_key');
    this.set('lastAccess', now);
    this.set('lastUpdate', now);
    internal.accessSid(key);
    sessions.replace(this);
    return this;
  },
  delete() {
    const now = Date.now();
    const key = this.get('_key');
    this.set('lastAccess', now);
    this.set('lastUpdate', now);
    try {
      internal.clearSid(key);
      deleteSession(key);
    } catch (e) {
      if (!(e instanceof errors.SessionNotFound)) {
        throw e;
      }
      return false;
    }
    return true;
  }
});

exports.create = createSession;
exports.get = Session.fromClient;
exports.delete = deleteSession;
exports.errors = errors;

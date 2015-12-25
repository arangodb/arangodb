'use strict';
const _ = require('lodash');
const joi = require('joi');
const internal = require('internal');
const arangodb = require('@arangodb');
const db = arangodb.db;
const Foxx = require('@arangodb/foxx');
const errors = require('./errors');

function getCollection() {
  return db._collection('_sessions');
}

const Session = Foxx.Model.extend({
  schema: {
    _key: joi.string().required(),
    uid: joi.string().allow(null).default(null),
    userData: joi.object().default(Object, 'Empty object'),
    sessionData: joi.object().default(Object, 'Empty object'),
    created: joi.number().integer().default(Date.now, 'Current date'),
    lastAccess: joi.number().integer().default(Date.now, 'Current date'),
    lastUpdate: joi.number().integer().default(Date.now, 'Current date')
  }
});

function generateSessionId() {
  return internal.genRandomAlphaNumbers(20);
}

function createSession(sessionData, userData) {
  const sid = generateSessionId();
  const session = new Session({
    _key: sid,
    uid: (userData && userData._id) || null,
    sessionData: sessionData || {},
    userData: userData || {},
    lastAccess: Date.now()
  });
  const meta = getCollection().save(session.attributes);
  session.set(meta);
  return session;
}

function deleteSession(sid) {
  try {
    getCollection().remove(sid);
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
  const collection = getCollection();
  let session;
  db._executeTransaction({
    collections: {
      read: [collection.name()],
      write: [collection.name()]
    },
    action() {
      try {
        session = new Session(collection.document(sid));

        const internalAccessTime = internal.accessSid(sid);
        if (internalAccessTime) {
          session.set('lastAccess', internalAccessTime);
        }
        session.enforceTimeout();

        const now = Date.now();
        session.set('lastAccess', now);
        const meta = collection.update(
          session.get('_key'),
          {lastAccess: now}
        );
        session.set(meta);
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
    const meta = getCollection().replace(this.attributes, this.attributes);
    this.set(meta);
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

'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
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
/// @author Alan Plum
////////////////////////////////////////////////////////////////////////////////

const assert = require('assert');
const internal = require('internal');
const arangodb = require('@arangodb');
const NOT_FOUND = arangodb.errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code;
const UNIQUE_CONSTRAINT = arangodb.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code;
const db = arangodb.db;
const aql = arangodb.aql;

module.exports = function systemStorage(cfg) {
  assert(!cfg, 'System session storage does not expect any options');
  const ttl = Number(internal.options()['server.session-timeout']) * 1000;
  return {
    prune() {
      return db._query(aql`
        FOR session IN _sessions
        FILTER (session.lastAccess + ${ttl}) < ${Date.now()}
        REMOVE session IN _sessions
        RETURN OLD._key
      `).toArray();
    },
    fromClient(sid) {
      try {
        const now = Date.now();
        const doc = db._sessions.document(sid);
        const internalAccessTime = internal.accessSid(sid);
        if (doc.uid && internalAccessTime) {
          doc.lastAccess = internalAccessTime;
        }
        const lastAccess = doc.lastAccess;
        if ((doc.lastAccess + ttl) < now) {
          this.clear(sid);
          return null;
        }
        if (doc.uid) {
          internal.clearSid(doc._key);
          internal.createSid(doc._key, doc.uid);
        }
        db._sessions.update(sid, {lastAccess: now});
        return {
          _key: doc._key,
          uid: doc.uid,
          lastAccess: lastAccess
        };
      } catch (e) {
        if (e.isArangoError && e.errorNum === NOT_FOUND) {
          return null;
        }
        throw e;
      }
    },
    forClient(session) {
      return session && session._key || null;
    },
    save(session) {
      if (!session) {
        return null;
      }
      const uid = session.uid;
      const payload = {
        uid: uid || null,
        lastAccess: Date.now()
      };
      let sid = session._key;
      const isNew = !sid;
      if (isNew) {
        // generate a new key
        let crypto = require("@arangodb/crypto");
        while (true) {
          payload._key = crypto.sha256(crypto.rand() + "-frontend");
          try {
            // test if key is already present in collection
            const meta = db._sessions.save(payload);
            sid = meta._key;
            session._key = sid;
            break;
          } catch (e) {
            if (!e.isArangoError || e.errorNum !== UNIQUE_CONSTRAINT) {
              throw e;
            }
          }
        }
      } else {
        db._sessions.replace(sid, payload);
      }
      if (uid) {
        internal.clearSid(session._key);
        internal.createSid(session._key, uid);
      }
      session.lastAccess = payload.lastAccess;
      return session;
    },
    setUser(session, uid) {
      if (uid) {
        session.uid = uid;
        if (session._key) {
          internal.clearSid(session._key);
          internal.createSid(session._key, uid);
        }
      } else {
        session.uid = null;
        if (session._key) {
          internal.clearSid(session._key);
        }
      }
    },
    clear(session) {
      if (!session || !session._key) {
        return false;
      }
      try {
        db._sessions.remove(session);
      } catch (e) {
        if (e.isArangoError && e.errorNum === NOT_FOUND) {
          internal.clearSid(session._key);
          return false;
        }
        throw e;
      }
      internal.clearSid(session._key);
      return true;
    },
    new() {
      return {
        uid: null,
        lastAccess: null
      };
    }
  };
};

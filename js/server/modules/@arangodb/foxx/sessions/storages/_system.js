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
const db = arangodb.db;
const aql = arangodb.aql;


module.exports = function systemStorage(cfg) {
  assert(!cfg, 'System session storage does not expect any options');
  const expiry = Number(internal.options()['server.session-timeout']) * 1000;
  return {
    prune() {
      return db._query(aql`
        FOR session IN _sessions
        FILTER session.lastAccess < ${Date.now() - expiry}
        REMOVE session IN _sessions
        RETURN OLD._key
      `).toArray();
    },
    fromClient(sid) {
      try {
        const doc = db._sessions.document(sid);
        const internalAccessTime = internal.accessSid(sid);
        if (internalAccessTime) {
          doc.lastAccess = internalAccessTime;
        }
        if ((doc.lastAccess + expiry) < Date.now()) {
          this.clear(sid);
          return null;
        }
        return {
          _key: doc._key,
          uid: doc.uid,
          userData: doc.userData,
          created: doc.created,
          data: doc.sessionData
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
        userData: session.userData || {},
        sessionData: session.data || {},
        created: session.created || Date.now(),
        lastAccess: Date.now(),
        lastUpdate: Date.now()
      };
      let sid = session._key;
      const isNew = !sid;
      if (isNew) {
        const meta = db._sessions.save(payload);
        sid = meta._key;
        session._key = sid;
      } else {
        db._sessions.replace(sid, payload);
      }
      if (uid) {
        if (isNew) {
          const user = db._users.document(uid);
          internal.createSid(session._key, user.user);
        } else {
          internal.accessSid(sid);
        }
      }
      return session;
    },
    setUser(session, user) {
      if (user) {
        session.uid = user._key;
        session.userData = user.userData;
        if (session._key) {
          internal.createSid(session._key, user.user);
        }
      } else {
        session.uid = null;
        session.userData = {};
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
        userData: {},
        created: Date.now(),
        data: {}
      };
    }
  };
};

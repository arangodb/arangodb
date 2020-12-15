'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const assert = require('assert');
const arangodb = require('@arangodb');
const NOT_FOUND = arangodb.errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code;
const UNIQUE_CONSTRAINT = arangodb.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code;
const db = arangodb.db;
const aql = arangodb.aql;

module.exports = function collectionStorage (cfg) {
  if (typeof cfg === 'string' || cfg.isArangoCollection) {
    cfg = {collection: cfg};
  }
  if (!cfg) {
    cfg = {};
  }
  const autoUpdate = Boolean(cfg.autoUpdate !== false);
  const pruneExpired = Boolean(cfg.pruneExpired);
  const ttl = (cfg.ttl || 60 * 60) * 1000;
  const collection = (
  typeof cfg.collection === 'string'
    ? db._collection(cfg.collection)
    : cfg.collection
  );
  assert(cfg.collection, 'Must pass a collection to store sessions');
  assert(collection.isArangoCollection, `No such collection: ${cfg.collection}`);
  return {
    prune() {
      return db._query(aql`
        FOR session IN ${collection}
        FILTER session.expires < DATE_NOW()
        REMOVE session IN ${collection}
        RETURN OLD._key
      `).toArray();
    },
    fromClient(sid) {
      try {
        const now = Date.now();
        const session = collection.document(sid);
        if (session.expires < now) {
          if (pruneExpired) {
            collection.remove(session);
          }
          return null;
        }
        if (autoUpdate) {
          collection.update(sid, {expires: now + ttl});
        }
        return {
          _key: session._key,
          uid: session.uid,
          created: session.created,
          data: session.data
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
      const payload = {
        uid: session.uid,
        created: session.created,
        expires: Date.now() + ttl,
        data: session.data
      };
      if (!session._key) {
        // generate a new key
        let crypto = require('@arangodb/crypto');
        while (true) {
          payload._key = crypto.sha256(crypto.rand() + '-frontend');
          try {
            // test if key is already present in collection
            const meta = collection.save(payload);
            session._key = meta._key;
            break;
          } catch (e) {
            if (!e.isArangoError || e.errorNum !== UNIQUE_CONSTRAINT) {
              throw e;
            }
          }
        }
      } else {
        collection.replace(session._key, payload);
      }
      return session;
    },
    clear(session) {
      if (!session || !session._key) {
        return false;
      }
      try {
        collection.remove(session);
      } catch (e) {
        if (e.isArangoError && e.errorNum === NOT_FOUND) {
          return false;
        }
        throw e;
      }
      return true;
    },
    new() {
      return {
        uid: null,
        created: Date.now(),
        data: null
      };
    }
  };
};

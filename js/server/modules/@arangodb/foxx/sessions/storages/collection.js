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

const arangodb = require('@arangodb');
const NOT_FOUND = arangodb.errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code;
const db = arangodb.db;


module.exports = function collectionStorage(cfg) {
  if (typeof cfg === 'string' || cfg.isArangoCollection) {
    cfg = {collection: cfg};
  }
  if (!cfg) {
    cfg = {};
  }
  const collection = (
    typeof cfg.collection === 'string'
    ? db._collection(cfg.collection)
    : cfg.collection
  );
  return {
    fromClient(sid) {
      try {
        return collection.document(sid);
      } catch (e) {
        if (e.isArangoError && e.errorNum === NOT_FOUND) {
          return null;
        }
        throw e;
      }
    },
    forClient(session) {
      const key = session._key;
      if (!key) {
        const meta = collection.save(session);
        return meta._key;
      }
      collection.replace(key, session);
      return key;
    },
    new() {
      return {};
    }
  };
};

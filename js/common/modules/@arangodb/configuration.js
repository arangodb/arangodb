/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// / Copyright 2014 triagens GmbH, Cologne, Germany
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Dr. Frank Celler, Lucas Dohmen
// //////////////////////////////////////////////////////////////////////////////

let db = require('@arangodb').db;
let internal = require('internal');

const cn = '_frontend';  
const key = 'notifications';

// //////////////////////////////////////////////////////////////////////////////
// / @brief the notifications configuration
// //////////////////////////////////////////////////////////////////////////////

exports.notifications = {};

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the versions notification
// //////////////////////////////////////////////////////////////////////////////

exports.notifications.versions = function () {
  let doc = {};

  let frontend = internal.db._collection(cn);
  if (frontend) {
    try {
      doc = frontend.document(key);
    } catch (err) {
      try {
        doc = frontend.insert({ _key: key });
      } catch (err) {}
    }
  }

  if (!doc.hasOwnProperty('versions')) {
    doc.versions = {};
  }
  if (internal.hasOwnProperty('frontendVersionCheck') && 
      !internal.frontendVersionCheck) {
    doc.enableVersionNotification = false;
  }

  return doc;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief sets the versions notification
// //////////////////////////////////////////////////////////////////////////////

exports.notifications.setVersions = function (data) {
  let frontend = internal.db._collection(cn);
  if (!frontend) {
    // collection not (yet) available
    return;
  }
  
  try {
    // assume document is there already
    frontend.update(key, data);
  } catch (err) {
    // probably not, so try to insert it
    data._key = key;
    frontend.insert(data);
  }
};

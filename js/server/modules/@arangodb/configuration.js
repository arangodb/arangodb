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

var db = require('@arangodb').db;
var internal = require('internal');
var shallowCopy = require('@arangodb/util').shallowCopy;

// //////////////////////////////////////////////////////////////////////////////
// / @brief the frontend collection
// //////////////////////////////////////////////////////////////////////////////

function getFrontendCollection () {
  var frontend = db._collection('_frontend');

  if (frontend === null) {
    throw new Error('_frontend collection not (yet) available');
  }
  return frontend;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief the notifications configuration
// //////////////////////////////////////////////////////////////////////////////

exports.notifications = {};

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the versions notification
// //////////////////////////////////////////////////////////////////////////////

exports.notifications.versions = function () {
  var n = 'notifications';
  var v = 'versions';
  var d;
  var frontend = getFrontendCollection();

  try {
    d = frontend.document(n);
  } catch (err) {
    try {
      d = frontend.save({ _key: n });
    } catch (err2) {
      d = {};
    }
  }

  d = shallowCopy(d);

  if (!d.hasOwnProperty(v)) {
    d.versions = {};
  }

  if (!internal.frontendVersionCheck) {
    d.enableVersionNotification = false;
  }

  return d;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief sets the versions notification
// //////////////////////////////////////////////////////////////////////////////

exports.notifications.setVersions = function (data) {
  var n = 'notifications';
  var d;
  var frontend = getFrontendCollection();

  try {
    d = frontend.document(n);
  } catch (err) {
    d = frontend.save({ _key: n });
  }

  frontend.update(n, data);
};

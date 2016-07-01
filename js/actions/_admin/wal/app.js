/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief administration actions
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
// / @author Dr. Frank Celler
// / @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
// / @author Copyright 2012-2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var internal = require('internal');
var actions = require('@arangodb/actions');

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_put_admin_wal_flush
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/wal/flush',
  prefix: false,

  callback: function (req, res) {
    if (req.requestType !== actions.PUT) {
      actions.resultUnsupported(req, res);
      return;
    }

    var body = actions.getJsonBody(req, res);
    if (body === undefined) {
      return;
    }

    var getParam = function (name) {
      // first check body value
      if (body.hasOwnProperty(name)) {
        return body[name];
      }

      // need to handle strings because parameter values are strings
      return (req.parameters.hasOwnProperty(name) &&
        (req.parameters[name] === 'true' || req.parameters[name] === true));
    };

    internal.wal.flush({
      waitForSync: getParam('waitForSync'),
      waitForCollector: getParam('waitForCollector')
    });

    actions.resultOk(req, res, actions.HTTP_OK);
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_put_admin_wal_properties
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_admin_wal_properties
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/wal/properties',
  prefix: false,

  callback: function (req, res) {
    var result;

    if (req.requestType === actions.PUT) {
      var body = actions.getJsonBody(req, res);
      if (body === undefined) {
        return;
      }

      result = internal.wal.properties(body);
      actions.resultOk(req, res, actions.HTTP_OK, result);
    } else if (req.requestType === actions.GET) {
      result = internal.wal.properties();
      actions.resultOk(req, res, actions.HTTP_OK, result);
    } else {
      actions.resultUnsupported(req, res);
    }
  }
});

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_admin_wal_transactions
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/wal/transactions',
  prefix: false,

  callback: function (req, res) {
    var result;

    if (req.requestType === actions.GET) {
      result = internal.wal.transactions();
      actions.resultOk(req, res, actions.HTTP_OK, result);
    } else {
      actions.resultUnsupported(req, res);
    }
  }
});

/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief global configuration
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
// / @author Jan Steemann
// / @author Copyright 2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var actions = require('@arangodb/actions');
var configuration = require('@arangodb/configuration');

// //////////////////////////////////////////////////////////////////////////////
// / @brief _admin/configuration/versions
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: '_admin/configuration/notifications/versions',
  prefix: false,

  callback: function (req, res) {
    var json;

    try {
      switch (req.requestType) {
        case actions.GET:
          actions.resultOk(req, res, actions.HTTP_OK, configuration.notifications.versions());
          break;

        case actions.PUT:
          json = actions.getJsonBody(req, res, actions.HTTP_BAD);

          if (json === undefined) {
            return;
          }

          configuration.notifications.setVersions(json);

          actions.resultOk(req, res, actions.HTTP_OK, {});
          break;

        default:
          actions.resultUnsupported(req, res);
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

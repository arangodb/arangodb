/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief querying and managing collections
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
// / @author Wilfried Goesgens
// / @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var arangodb = require('@arangodb');
var actions = require('@arangodb/actions');
var tasks = require('@arangodb/tasks');

var API = '_api/tasks';

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_api_tasks
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_api_tasks_all
// //////////////////////////////////////////////////////////////////////////////

function get_api_tasks (req, res) {
  if (req.suffix.length === 1) {
    var matchId = decodeURIComponent(req.suffix[0]);
    var allTasks = tasks.get().filter(function (task) {
      return (task.id === matchId);
    });

    if (allTasks.length === 0) {
      actions.resultNotFound(req, res, arangodb.ERROR_TASK_NOT_FOUND);
    } else {
      actions.resultOk(req, res, actions.HTTP_OK, allTasks[0]);
    }
  } else {
    actions.resultOk(req, res, actions.HTTP_OK, tasks.get());
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_post_api_new_tasks
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_put_api_new_tasks
// //////////////////////////////////////////////////////////////////////////////

function post_api_task_register (req, res, byId) {
  var body = actions.getJsonBody(req, res);
  if (byId) {
    if (req.suffix.length !== 1) {
      actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
        'expected PUT /' + API + '/<task-id>');
      return;
    } else {
      body.id = decodeURIComponent(req.suffix[0]);
    }
  }

  try {
    var result = tasks.register(body);

    actions.resultOk(req, res, actions.HTTP_OK, result);
  } catch (err) {
    actions.resultException(req, res, err, undefined, false);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_delete_api_tasks
// //////////////////////////////////////////////////////////////////////////////

function delete_api_task (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
      'expected DELETE /' + API + '/<task-id>');
  } else {
    try {
      tasks.unregister(decodeURIComponent(req.suffix[0]));

      actions.resultOk(req, res, actions.HTTP_OK, {});
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief handles a collection request
// //////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: API,

  callback: function (req, res) {
    try {
      if (req.requestType === actions.GET) {
        get_api_tasks(req, res);
      } else if (req.requestType === actions.DELETE) {
        delete_api_task(req, res);
      } else if (req.requestType === actions.POST) {
        post_api_task_register(req, res, false);
      } else if (req.requestType === actions.PUT) {
        post_api_task_register(req, res, true);
      } else {
        actions.resultUnsupported(req, res);
      }
    } catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

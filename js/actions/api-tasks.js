/*jshint strict: false */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief querying and managing collections
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Wilfried Goesgens
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");
var tasks = require("org/arangodb/tasks");

var API = "_api/tasks";

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_get_api_tasks
/// @brief Retrieves all (or one) currently active server task
///
/// @RESTHEADER{GET /_api/tasks/{id}, Fetch all tasks or one task with id}
/// 
/// @RESTURLPARAM{id,string,optional}
/// The id of the task to fetch.
///
/// @RESTDESCRIPTION
/// fetches all existing tasks on the server; optionally just the one specified
/// by *id*
///
/// @EXAMPLES
///
/// Getting all tasks:
/// @EXAMPLE_ARANGOSH_RUN{RestTasksListAll}
///     var url = "/_api/tasks";
///
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Getting a single task by id:
/// @EXAMPLE_ARANGOSH_RUN{RestTasksListOne}
///     var url = "/_api/tasks/statistics-average-collector";
///
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 200);
///     logJsonResponse(response);
///
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Fetching a non-existing task:
/// @EXAMPLE_ARANGOSH_RUN{RestTasksListNonExisting}
///     var url = "/_api/tasks/non-existing-task";
///
///     var response = logCurlRequest('GET', url);
///
///     assert(response.code === 404);
///     logJsonResponse(response);
///
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function get_api_tasks (req, res) {
  if (req.suffix.length === 1) {
    var matchId = decodeURIComponent(req.suffix[0]);
    var allTasks = tasks.get().filter(function (task) {
      return (task.id === matchId);
    });

    if (allTasks.length === 0) {
      actions.resultNotFound(req, res, arangodb.ERROR_TASK_NOT_FOUND);
    }
    else {
      actions.resultOk(req, res, actions.HTTP_OK, allTasks[0]);
    }
  }
  else {
    actions.resultOk(req, res, actions.HTTP_OK, tasks.get());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_post_api_new_tasks
/// @brief creates a new task
///
/// @RESTHEADER{POST /_api/tasks, creates a task}
///
/// @RESTBODYPARAM{body,json,required}
/// the body with the new task:
/// - *name*: name of the task
/// - *command*: javascript code to be executed
/// - *params*: parameters to be passed into command
/// - *period*: n seconds between the executions
/// - *offset*: n seconds delay 
///
/// @RESTDESCRIPTION
/// creates a new task under a generated id
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{400}
/// If the post body is not accurate, a *HTTP 400* is returned.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestTasksCreate}
///     var url = "/_api/tasks/";
///
///     // Note: prints stuff if server is running in non-daemon mode.
///     var sampleTask = {
///       name:"SampleTask",
///       command: "(function(params) { require('internal').print(params); })(params)",
///       params: { "foo": "bar", "bar": "foo"},
///       period:2
///     }
///     var response = logCurlRequest('POST', url,
///                                   JSON.stringify(sampleTask));
///
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///
///     // Cleanup:
///     logCurlRequest('DELETE', url + JSON.parse(response.body).id);
///
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_put_api_new_tasks
/// @brief registers a new task with a pre-defined Id
///
/// @RESTHEADER{PUT /_api/tasks/{id}, creates a task with id}
/// 
/// @RESTURLPARAM{id,string,required}
/// The id of the task to fetch.
///
/// @RESTBODYPARAM{body,json,required}
/// the body with the new task:
/// - *name*: name of the task
/// - *command*: javascript code to be executed
/// - *params*: parameters to be passed into command
/// - *period*: n seconds between the executions
/// - *offset*: n seconds delay 
///
/// @RESTDESCRIPTION
/// registers a new task under id
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{400}
/// If the task *id* already exists or the rest body is not accurate, *HTTP 400* is returned.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestTasksPutWithId}
///     var url = "/_api/tasks/";
///
///     // Note: prints stuff if server is running in non-daemon mode.
///     var sampleTask = {
///       id: "SampleTask",
///       name: "SampleTask",
///       command: "(function(params) { require('internal').print(params); })(params)",
///       params: { "foo": "bar", "bar": "foo"},
///       period: 2
///     }
///     var response = logCurlRequest('PUT', url + 'sampleTask',
///                                   JSON.stringify(sampleTask));
///     assert(response.code === 200);
///
///     logJsonResponse(response);
///
///     // Cleanup:
///     curlRequest('DELETE', url + 'sampleTask');
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function post_api_task_register (req, res, byId) {
  var body = actions.getJsonBody(req, res);
  if (byId) {
    if (req.suffix.length !== 1) {
      actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                        "expected PUT /" + API + "/<task-id>");
      return;
    }
    else {
      body.id = decodeURIComponent(req.suffix[0]);
    }
  }

  try {
    var result = tasks.register(body);

    actions.resultOk(req, res, actions.HTTP_OK, result);
  }
  catch (err) {
    actions.resultException(req, res, err, undefined, false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_delete_api_tasks
/// @brief deletes one currently active server task
///
/// @RESTHEADER{DELETE /_api/tasks/{id}, deletes the task with id}
/// 
/// @RESTURLPARAM{id,string,required}
/// The id of the task to delete.
///
/// @RESTDESCRIPTION
/// Deletes the task identified by *id* on the server. 
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{404}
/// If the task *id* is unknown, then an *HTTP 404* is returned.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_RUN{RestTasksDeleteFail}
///     var url = "/_api/tasks/NoTaskWithThatName";
///
///     var response = logCurlRequest('DELETE', url);
///
///     assert(response.code === 404);
///
///     logJsonResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// @EXAMPLE_ARANGOSH_RUN{RestTasksDelete}
///     var url = "/_api/tasks/";
///
///     var sampleTask = {
///       id: "SampleTask",
///       name: "SampleTask",
///       command: "2+2;",
///       period: 2
///     }
///     // Ensure it's really not there:
///     curlRequest('DELETE', url + sampleTask.id);
///     // put in something we may delete:
///     curlRequest('PUT', url + sampleTask.id,
///                 JSON.stringify(sampleTask));
///
///     var response = logCurlRequest('DELETE', url + sampleTask.id);
///
///     assert(response.code === 200);
///     logJsonResponse(response);
///
/// @END_EXAMPLE_ARANGOSH_RUN
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function delete_api_task (req, res) {
  if (req.suffix.length !== 1) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER,
                      "expected DELETE /" + API + "/<task-id>");
  }
  else {
    try {
      var result = tasks.unregister(decodeURIComponent(req.suffix[0]));

      actions.resultOk(req, res, actions.HTTP_OK, result);
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a collection request
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : API,

  callback : function (req, res) {
    try {
      if (req.requestType === actions.GET) {
        get_api_tasks(req, res);
      }
      else if (req.requestType === actions.DELETE) {
        delete_api_task(req, res);
      }
      else if (req.requestType === actions.POST) {
        post_api_task_register(req, res, false);
      }
      else if (req.requestType === actions.PUT) {
        post_api_task_register(req, res, true);
      }
      else {
        actions.resultUnsupported(req, res);
      }
    }
    catch (err) {
      actions.resultException(req, res, err, undefined, false);
    }
  }
});

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

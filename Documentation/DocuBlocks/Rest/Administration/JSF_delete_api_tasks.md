
@startDocuBlock JSF_delete_api_tasks
@brief deletes one currently active server task

@RESTHEADER{DELETE /_api/tasks/{id}, deletes the task with id}

@RESTURLPARAM{id,string,required}
The id of the task to delete.

@RESTDESCRIPTION
Deletes the task identified by *id* on the server. 

@RESTRETURNCODES

@RESTRETURNCODE{404}
If the task *id* is unknown, then an *HTTP 404* is returned.

@EXAMPLES

trying to delete non existing task

@EXAMPLE_ARANGOSH_RUN{RestTasksDeleteFail}
    var url = "/_api/tasks/NoTaskWithThatName";

    var response = logCurlRequest('DELETE', url);

    assert(response.code === 404);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN

Remove existing Task

@EXAMPLE_ARANGOSH_RUN{RestTasksDelete}
    var url = "/_api/tasks/";

    var sampleTask = {
      id: "SampleTask",
      name: "SampleTask",
      command: "2+2;",
      period: 2
    }
    // Ensure it's really not there:
    curlRequest('DELETE', url + sampleTask.id);
    // put in something we may delete:
    curlRequest('PUT', url + sampleTask.id,
                sampleTask);

    var response = logCurlRequest('DELETE', url + sampleTask.id);

    assert(response.code === 200);
    logJsonResponse(response);

@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock



@startDocuBlock delete_api_tasks
@brief deletes one currently active server task

@RESTHEADER{DELETE /_api/tasks/{id}, deletes the task with id, deleteTask}

@RESTURLPARAM{id,string,required}
The id of the task to delete.

@RESTDESCRIPTION
Deletes the task identified by *id* on the server.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If the task was deleted, *HTTP 200* is returned.

@RESTREPLYBODY{code,number,required,}
The status code, 200 in this case.

@RESTREPLYBODY{error,boolean,required,}
*false* in this case

@RESTRETURNCODE{404}
If the task *id* is unknown, then an *HTTP 404* is returned.

@RESTREPLYBODY{code,number,required,}
The status code, 404 in this case.

@RESTREPLYBODY{error,boolean,required,}
*true* in this case

@RESTREPLYBODY{errorMessage,string,required,}
A plain text message stating what went wrong.

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
    curlRequest('DELETE', url + sampleTask.id, null, null, [404,200]);
    // put in something we may delete:
    curlRequest('PUT', url + sampleTask.id,
                sampleTask);

    var response = logCurlRequest('DELETE', url + sampleTask.id);

    assert(response.code === 200);
    logJsonResponse(response);

@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

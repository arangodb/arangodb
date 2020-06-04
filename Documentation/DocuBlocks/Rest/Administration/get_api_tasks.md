
@startDocuBlock get_api_tasks
@brief Retrieves one currently active server task

@RESTHEADER{GET /_api/tasks/{id}, Fetch one task with id, getTask}

@RESTURLPARAM{id,string,required}
The id of the task to fetch.

@RESTDESCRIPTION
fetches one existing task on the server specified by *id*

@RESTRETURNCODE{200}
The requested task

@RESTREPLYBODY{,object,required,api_task_struct}
The function in question

@EXAMPLES

Fetching a single task by its id
@EXAMPLE_ARANGOSH_RUN{RestTasksListOne}
    var url = "/_api/tasks";
    var response = logCurlRequest('POST', url, JSON.stringify({ id: "testTask", command: "console.log('Hello from task!');", offset: 10000 }));

    var response = logCurlRequest('GET', url + "/testTask");

    assert(response.code === 200);
    logJsonResponse(response);

@END_EXAMPLE_ARANGOSH_RUN

Trying to fetch a non-existing task
@EXAMPLE_ARANGOSH_RUN{RestTasksListNonExisting}
    var url = "/_api/tasks/non-existing-task";

    var response = logCurlRequest('GET', url);

    assert(response.code === 404);
    logJsonResponse(response);

@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

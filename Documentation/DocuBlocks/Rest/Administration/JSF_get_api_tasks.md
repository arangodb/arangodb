
@startDocuBlock JSF_get_api_tasks
@brief Retrieves  one currently active server task

@RESTHEADER{GET /_api/tasks/{id}, Fetch one task with id}

@RESTURLPARAM{id,string,required}
The id of the task to fetch.

@RESTDESCRIPTION
fetches one existing tasks on the server specified by *id*

@RESTRETURNCODE{200}
The requested task

@EXAMPLES

Fetching a single task by its id
@EXAMPLE_ARANGOSH_RUN{RestTasksListOne}
    var url = "/_api/tasks/statistics-average-collector";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);
    logJsonResponse(response);

@END_EXAMPLE_ARANGOSH_RUN

trying to fetch a non-existing task
@EXAMPLE_ARANGOSH_RUN{RestTasksListNonExisting}
    var url = "/_api/tasks/non-existing-task";

    var response = logCurlRequest('GET', url);

    assert(response.code === 404);
    logJsonResponse(response);

@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock


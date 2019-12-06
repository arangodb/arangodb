
@startDocuBlock get_api_tasks_all
@brief Retrieves all currently active server tasks

@RESTHEADER{GET /_api/tasks/, Fetch all tasks or one task, getTasks}

@RESTDESCRIPTION
fetches all existing tasks on the server

@RESTRETURNCODE{200}
The list of tasks

@RESTREPLYBODY{,array,required,api_task_struct}
a list of all tasks

@EXAMPLES

Fetching all tasks
@EXAMPLE_ARANGOSH_RUN{RestTasksListAll}
    var url = "/_api/tasks";

    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);

@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock


@startDocuBlock put_api_new_tasks
@brief registers a new task with a pre-defined id; not compatible with load balancers

@RESTHEADER{PUT /_api/tasks/{id}, creates a task with id, registerTask:byId}

@RESTURLPARAM{id,string,required}
The id of the task to create

@RESTBODYPARAM{name,string,required,string}
The name of the task

@RESTBODYPARAM{command,string,required,string}
The JavaScript code to be executed

@RESTBODYPARAM{params,string,required,string}
The parameters to be passed into command

@RESTBODYPARAM{period,integer,optional,int64}
number of seconds between the executions

@RESTBODYPARAM{offset,integer,optional,int64}
Number of seconds initial delay

@RESTDESCRIPTION
registers a new task with the specified id

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the task *id* already exists or the rest body is not accurate, *HTTP 400* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestTasksPutWithId}
    var url = "/_api/tasks/";

    // Note: prints stuff if server is running in non-daemon mode.
    var sampleTask = {
      id: "SampleTask",
      name: "SampleTask",
      command: "(function(params) { require('@arangodb').print(params); })(params)",
      params: { "foo": "bar", "bar": "foo"},
      period: 2
    }
    var response = logCurlRequest('PUT', url + 'sampleTask',
                                  sampleTask);
    assert(response.code === 200);

    logJsonResponse(response);

    // Cleanup:
    curlRequest('DELETE', url + 'sampleTask');
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

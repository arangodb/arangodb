
@startDocuBlock JSF_post_api_new_tasks
@brief creates a new task

@RESTHEADER{POST /_api/tasks, creates a task}

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
creates a new task with a generated id

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the post body is not accurate, a *HTTP 400* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestTasksCreate}
    var url = "/_api/tasks/";

    // Note: prints stuff if server is running in non-daemon mode.
    var sampleTask = {
      name: "SampleTask",
      command: "(function(params) { require('@arangodb').print(params); })(params)",
      params: { "foo": "bar", "bar": "foo"},
      period: 2
    }
    var response = logCurlRequest('POST', url,
                                  sampleTask);

    assert(response.code === 200);

    logJsonResponse(response);

    // Cleanup:
    logCurlRequest('DELETE', url + JSON.parse(response.body).id);

@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock


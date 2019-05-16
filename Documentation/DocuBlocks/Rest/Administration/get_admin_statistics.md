
@startDocuBlock get_admin_statistics
@brief return the statistics information

@RESTHEADER{GET /_admin/statistics, Read the statistics, getStatistics}

@RESTDESCRIPTION
Returns the statistics information. The returned object contains the
statistics figures grouped together according to the description returned by
*_admin/statistics-description*. For instance, to access a figure *userTime*
from the group *system*, you first select the sub-object describing the
group stored in *system* and in that sub-object the value for *userTime* is
stored in the attribute of the same name.

In case of a distribution, the returned object contains the total count in
*count* and the distribution list in *counts*. The sum (or total) of the
individual values is returned in *sum*.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Statistics were returned successfully.

@RESTREPLYBODY{error,boolean,required,}
boolean flag to indicate whether an error occurred (*false* in this case)

@RESTREPLYBODY{code,integer,required,int64}
the HTTP status code - 200 in this case

@RESTREPLYBODY{time,integer,required,int64}
the current server timestamp

@RESTREPLYBODY{errorMessage,string,required,string}
a descriptive error message

@RESTREPLYBODY{enabled,boolean,required,}
*true* if the server has the statistics module enabled. If not, don't expect any values.


@RESTREPLYBODY{system,object,required,system_statistics_struct}
metrics gathered from the system about this process; may depend on the host OS

@RESTSTRUCT{minorPageFaults,system_statistics_struct,integer,required,}
pagefaults

@RESTSTRUCT{majorPageFaults,system_statistics_struct,integer,required,}
pagefaults

@RESTSTRUCT{userTime,system_statistics_struct,number,required,float}
the user CPU time used by the server process

@RESTSTRUCT{systemTime,system_statistics_struct,number,required,float}
the system CPU time used by the server process

@RESTSTRUCT{numberOfThreads,system_statistics_struct,integer,required,}
the number of threads in the server

@RESTSTRUCT{residentSize,system_statistics_struct,integer,required,}
RSS of process

@RESTSTRUCT{residentSizePercent,system_statistics_struct,number,required,float}
RSS of process in %

@RESTSTRUCT{virtualSize,system_statistics_struct,integer,required,}
VSS of the process


@RESTREPLYBODY{client,object,required,client_statistics_struct}
information about the connected clients and their resource usage


@RESTSTRUCT{sum,setof_statistics_struct,number,required,}
summarized value of all counts

@RESTSTRUCT{count,setof_statistics_struct,integer,required,}
number of values summarized

@RESTSTRUCT{counts,setof_statistics_struct,array,required,integer}
array containing the values


@RESTSTRUCT{connectionTime,client_statistics_struct,object,required,setof_statistics_struct}
total connection times

@RESTSTRUCT{totalTime,client_statistics_struct,object,required,setof_statistics_struct}
the system time 

@RESTSTRUCT{requestTime,client_statistics_struct,object,required,setof_statistics_struct}
the request times

@RESTSTRUCT{queueTime,client_statistics_struct,object,required,setof_statistics_struct}
the time requests were queued waiting for processing

@RESTSTRUCT{ioTime,client_statistics_struct,object,required,setof_statistics_struct}
IO Time

@RESTSTRUCT{bytesSent,client_statistics_struct,object,required,setof_statistics_struct}
number of bytes sent to the clients

@RESTSTRUCT{bytesReceived,client_statistics_struct,object,required,setof_statistics_struct}
number of bytes received from the clients


@RESTSTRUCT{httpConnections,client_statistics_struct,integer,required,}
the number of open http connections



@RESTREPLYBODY{http,object,required,http_statistics_struct}
the numbers of requests by Verb

@RESTSTRUCT{requestsTotal,http_statistics_struct,integer,required,}
total number of http requests

@RESTSTRUCT{requestsAsync,http_statistics_struct,integer,required,}
total number of asynchronous http requests

@RESTSTRUCT{requestsGet,http_statistics_struct,integer,required,}
No of requests using the GET-verb

@RESTSTRUCT{requestsHead,http_statistics_struct,integer,required,}
No of requests using the HEAD-verb

@RESTSTRUCT{requestsPost,http_statistics_struct,integer,required,}
No of requests using the POST-verb

@RESTSTRUCT{requestsPut,http_statistics_struct,integer,required,}
No of requests using the PUT-verb

@RESTSTRUCT{requestsPatch,http_statistics_struct,integer,required,}
No of requests using the PATCH-verb

@RESTSTRUCT{requestsDelete,http_statistics_struct,integer,required,}
No of requests using the DELETE-verb

@RESTSTRUCT{requestsOptions,http_statistics_struct,integer,required,}
No of requests using the OPTIONS-verb

@RESTSTRUCT{requestsOther,http_statistics_struct,integer,required,}
No of requests using the none of the above identified verbs


@RESTREPLYBODY{server,object,required,server_statistics_struct}
statistics of the server

@RESTSTRUCT{uptime,server_statistics_struct,integer,required,}
time the server is up and running

@RESTSTRUCT{physicalMemory,server_statistics_struct,integer,required,}
available physical memory on the server


@RESTSTRUCT{v8Context,server_statistics_struct,object,required,v8_context_struct}
Statistics about the V8 javascript contexts

@RESTSTRUCT{available,v8_context_struct,integer,required,}
the number of currently spawnen V8 contexts

@RESTSTRUCT{busy,v8_context_struct,integer,required,}
the number of currently active V8 contexts

@RESTSTRUCT{dirty,v8_context_struct,integer,required,}
the number of contexts that were previously used, and should now be garbage collected before being re-used

@RESTSTRUCT{free,v8_context_struct,integer,required,}
the number of V8 contexts that are free to use

@RESTSTRUCT{max,v8_context_struct,integer,required,}
the total number of V8 contexts we may spawn as configured by --javascript.v8-contexts


@RESTSTRUCT{threads,server_statistics_struct,object,required,server_threads_struct}
Statistics about the server worker threads (excluding V8 specific or jemalloc specific threads and system threads)

@RESTSTRUCT{scheduler-threads,server_threads_struct,integer,required,}
The number of spawned worker threads

@RESTSTRUCT{in-progress,server_threads_struct,integer,required,}
The number of currently busy worker threads

@RESTSTRUCT{queued,server_threads_struct,integer,required,}
The number of jobs queued up waiting for worker threads becomming available

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestAdminStatistics1}
    var url = "/_admin/statistics";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock


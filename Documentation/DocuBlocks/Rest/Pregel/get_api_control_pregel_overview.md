@startDocuBlock get_api_control_pregel_overview
@brief Get the overview of currently running Pregel jobs

@RESTHEADER{GET /_api/control_pregel, Get currently running Pregel jobs}

@RESTDESCRIPTION
Returns a list of currently running and recently finished Pregel jobs without
retrieving their results.
The returned object is a JSON array of Pregel job descriptions. Each job description
is a JSON object with the following attributes:

- *id*: an id of the Pregel job, as a string.

- *algorithm*: an algorithm used by the job.

- *created*: a date and time when the job was created.

- *expires*: a date and time when the job results expire. The expiration date is only
  meaningful for jobs that were completed, canceled or resulted in an error. Such jobs
  are cleaned up by the garbage collection when they reach their expiration date/time.

- *ttl*: a TTL (time to live) value for job results, specified in seconds.
  The TTL is used to calculate the expiration date for the job's results.

- *state*: a state of the execution, as a string.

- *gss*: a number of global supersteps executed.

- *totalRuntime*: a total runtime of the execution up to now (if the execution is still ongoing).

- *startupTime*: a startup runtime of the execution. The startup time includes the data 
  loading time and can be substantial.
  The startup time is reported as 0, if the startup is still ongoing.

- *computationTime*: an algorithm execution time. The computation time is reported as 0, if the 
  computation is still ongoing.

- *storageTime*: time for storing the results if the job includes result storage.
  The storage time is reported as 0, if storing the results is still ongoing.

- *reports*: optional statistics about the Pregel execution. The value is only populated when
  the algorithm has finished.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned when the list of jobs can be retrieved successfully.

@endDocuBlock

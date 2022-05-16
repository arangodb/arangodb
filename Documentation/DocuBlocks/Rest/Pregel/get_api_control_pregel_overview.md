@startDocuBlock get_api_control_pregel_overview
@brief Get the overview of currently running Pregel jobs

@RESTHEADER{GET /_api/control_pregel, Get currently running Pregel jobs}

@RESTDESCRIPTION
Returns the list of currently running Pregel jobs and those who recently
finished without their result having been retrieved.
The result is a JSON array of Pregel job descriptions. Each job description
is a JSON object with the following attributes:

- *id*: id of the Pregel job, as a string.

- *algorithm*: the algorithm used by the job.

- *created*: the date and time the job was created.

- *expires*: the date and time the job results will expire. The expiration date is only
  meaningful for jobs that have completed, errored or were canceled. Such jobs
  will be cleaned up by the garbage collection when they reach their expire date/time.

- *ttl*: the TTL (time to live) value for the job results, specified in seconds.
  The TTL is used to calculate the expire date for the job's results.

- *state*: the state of the execution, as a string.

- *gss*: the number of global supersteps executed.

- *totalRuntime*: total runtime of the execution up to now (if the execution is still ongoing).

- *startupTime*: startup runtime of the execution. The startup time includes the data 
  loading time and can be substantial.
  The startup time will be reported as 0 if the startup is still ongoing.

- *computationTime*: algorithm execution time. The computation time will be reported as 0 if the 
  computation still ongoing.

- *storageTime*: time for storing the results if the job includes results storage.
  The storage time be reported as 0 if storing the results is still ongoing.

- *reports*: optional statistics about the Pregel execution. The value will only be populated once
  the algorithm has finished.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned when the list of jobs can be retrieved successfully.

@endDocuBlock

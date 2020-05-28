
@startDocuBlock get_admin_time
@brief Get the current time of the system

@RESTHEADER{GET /_admin/time, Return system time, RestTimeHandler}

@RESTDESCRIPTION
The call returns an object with the attribute *time*. This contains the
current system time as a Unix timestamp with microsecond precision.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Time was returned successfully.

@RESTREPLYBODY{error,boolean,required,}
boolean flag to indicate whether an error occurred (*false* in this case)

@RESTREPLYBODY{code,integer,required,int64}
the HTTP status code

@RESTREPLYBODY{time,number,required,float}
The current system time as a Unix timestamp with microsecond precision of the server

@endDocuBlock

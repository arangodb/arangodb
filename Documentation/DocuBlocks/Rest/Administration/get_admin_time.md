
@startDocuBlock get_admin_time

@RESTHEADER{GET /_admin/time, Get the system time, getTime}

@RESTDESCRIPTION
The call returns an object with the `time` attribute. This contains the
current system time as a Unix timestamp with microsecond precision.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Time was returned successfully.

@RESTREPLYBODY{error,boolean,required,}
boolean flag to indicate whether an error occurred (`false` in this case)

@RESTREPLYBODY{code,integer,required,int64}
the HTTP status code

@RESTREPLYBODY{time,number,required,float}
The current system time as a Unix timestamp with microsecond precision of the server

@endDocuBlock

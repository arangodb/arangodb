
@startDocuBlock JSF_get_admin_time
@brief Get the current time of the system

@RESTHEADER{GET /_admin/time, Return system time}

@RESTDESCRIPTION

The call returns an object with the attribute *time*. This contains the
current system time as a Unix timestamp with microsecond precision.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Time was returned successfully.
@endDocuBlock


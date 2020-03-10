
@startDocuBlock delete_api_shutdown
@brief initiates the shutdown sequence

@RESTHEADER{DELETE /_admin/shutdown, Initiate shutdown sequence, RestShutdownHandler}

@RESTDESCRIPTION
This call initiates a clean shutdown sequence. Requires administrive privileges

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned in all cases, `OK` will be returned in the result buffer on success.

@endDocuBlock


@startDocuBlock JSF_post_batch_replication
@brief handle a dump batch command

@RESTHEADER{POST /_api/replication/batch, Create new dump batch}

**Note**: These calls are uninteresting to users.

@RESTBODYPARAM{ttl,integer,required,int64}
the time-to-live for the new batch (in seconds)

A JSON object with the batch configuration.

@RESTDESCRIPTION
Creates a new dump batch and returns the batch's id.

The response is a JSON object with the following attributes:

- *id*: the id of the batch

**Note**: on a coordinator, this request must have the query parameter
*DBserver* which must be an ID of a DBserver.
The very same request is forwarded synchronously to that DBserver.
It is an error if this attribute is not bound in the coordinator case.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the batch was created successfully.

@RESTRETURNCODE{400}
is returned if the ttl value is invalid or if *DBserver* attribute
is not specified or illegal on a coordinator.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.
@endDocuBlock


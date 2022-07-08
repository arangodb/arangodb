
@startDocuBlock post_batch_replication
@brief handle a dump batch command

@RESTHEADER{POST /_api/replication/batch, Create new dump batch, handleCommandBatch:Create}

**Note**: These calls are uninteresting to users.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{state,boolean,optional}
setting `state` to true will make the response also contain
a `state` attribute with information about the leader state.
This is used only internally during the replication process 
and should not be used by client applications.

@RESTBODYPARAM{ttl,integer,required,int64}
the time-to-live for the new batch (in seconds)<br>
A JSON object with the batch configuration.

@RESTDESCRIPTION
Creates a new dump batch and returns the batch's id.

The response is a JSON object with the following attributes:

- *id*: the id of the batch
- *lastTick*: snapshot tick value using when creating the batch
- *state*: additional leader state information (only present if the
  `state` URL parameter was set to `true` in the request)

**Note**: on a Coordinator, this request must have the query parameter
*DBserver* which must be an ID of a DB-Server.
The very same request is forwarded synchronously to that DB-Server.
It is an error if this attribute is not bound in the Coordinator case.

@RESTRETURNCODES

@RESTRETURNCODE{200}
is returned if the batch was created successfully.

@RESTRETURNCODE{400}
is returned if the ttl value is invalid or if *DBserver* attribute
is not specified or illegal on a Coordinator.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.
@endDocuBlock

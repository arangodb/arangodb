
@startDocuBlock put_batch_replication
@brief handle a dump batch command

@RESTHEADER{PUT /_api/replication/batch/{id}, Prolong existing dump batch, handleCommandBatch:Prolong}

**Note**: These calls are uninteresting to users.

@RESTBODYPARAM{ttl,integer,required,int64}
the time-to-live for the new batch (in seconds)

@RESTURLPARAMETERS

@RESTURLPARAM{id,string,required}
The id of the batch.

@RESTDESCRIPTION
Extends the ttl of an existing dump batch, using the batch's id and
the provided ttl value.

If the batch's ttl can be extended successfully, the response is empty.

**Note**: on a Coordinator, this request must have the query parameter
*DBserver* which must be an ID of a DB-Server.
The very same request is forwarded synchronously to that DB-Server.
It is an error if this attribute is not bound in the Coordinator case.

@RESTRETURNCODES

@RESTRETURNCODE{204}
is returned if the batch's ttl was extended successfully.

@RESTRETURNCODE{400}
is returned if the ttl value is invalid or the batch was not found.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.
@endDocuBlock

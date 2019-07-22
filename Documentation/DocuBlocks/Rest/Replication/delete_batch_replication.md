
@startDocuBlock delete_batch_replication
@brief handle a dump batch command

@RESTHEADER{DELETE /_api/replication/batch/{id}, Deletes an existing dump batch,handleCommandBatch:DELETE}

**Note**: These calls are uninteresting to users.

@RESTURLPARAMETERS

@RESTURLPARAM{id,string,required}
The id of the batch.

@RESTDESCRIPTION
Deletes the existing dump batch, allowing compaction and cleanup to resume.

**Note**: on a coordinator, this request must have the query parameter
*DBserver* which must be an ID of a DBserver.
The very same request is forwarded synchronously to that DBserver.
It is an error if this attribute is not bound in the coordinator case.

@RESTRETURNCODES

@RESTRETURNCODE{204}
is returned if the batch was deleted successfully.

@RESTRETURNCODE{400}
is returned if the batch was not found.

@RESTRETURNCODE{405}
is returned when an invalid HTTP method is used.
@endDocuBlock


@startDocuBlock api_foxx_commit
@brief commit local service state

@RESTHEADER{POST /_api/foxx/commit, Commit local service state}

@RESTDESCRIPTION
Commits the local service state of the Coordinator to the database.

This can be used to resolve service conflicts between Coordinators that can not be fixed automatically due to missing data.

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{replace,boolean,optional}
Overwrite existing service files in database even if they already exist.

@RESTRETURNCODE{204}
Returned if the request was successful.

@endDocuBlock

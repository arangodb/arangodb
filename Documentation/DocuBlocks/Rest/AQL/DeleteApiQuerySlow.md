
@startDocuBlock DeleteApiQuerySlow
@brief clears the list of slow AQL queries

@RESTHEADER{DELETE /_api/query/slow, Clears the list of slow AQL queries, deleteQuery}

@RESTDESCRIPTION
Clears the list of slow AQL queries

@RESTRETURNCODES

@RESTRETURNCODE{200}
The server will respond with *HTTP 200* when the list of queries was
cleared successfully.

@RESTRETURNCODE{400}
The server will respond with *HTTP 400* in case of a malformed request.
@endDocuBlock


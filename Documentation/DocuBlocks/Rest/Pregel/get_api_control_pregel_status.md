@startDocuBlock get_api_control_pregel_status
@brief Get the status of a Pregel execution

@RESTHEADER{GET /_api/control_pregel/{id}, Get Pregel execution status}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{id,number,required}
Pregel execution identifier.

@RESTDESCRIPTION
Returns the current state of the execution, the current global superstep, the
runtime, the global aggregator values as well as the number of send and
received messages.

<!--
State	Description
"running"	Algorithm is executing normally.
"in error"	The execution is in an error state. This can be caused by primary DB-Servers being not reachable or being non responsive. The execution might recover later, or switch to “canceled” if it was not able to recover successfully
"recovering"	The execution is actively recovering, will switch back to “running” if the recovery was successful
"canceled"	The execution was permanently canceled, either by the user or by an error.
"storing"	The algorithm finished, but the results are still being written back into the collections. Occurs if the store parameter is set to true only.
"done"	The execution is done. In version 3.7.1 and later, this means that storing is also done. In earlier versions, the results may not be written back into the collections yet. This event is announced in the server log (requires at least info log level for the pregel topic).
-->

@RESTRETURNCODES

@RESTRETURNCODE{200}
TODO

@RESTREPLYBODY{state,string,required,string}
State of the execution.

@RESTRETURNCODE{XXX}
TODO

<!--
BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES
SERVER_ERROR, TRI_ERROR_INTERNAL, pregel feature not available
NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND, Execution number is invalid
-->

@endDocuBlock

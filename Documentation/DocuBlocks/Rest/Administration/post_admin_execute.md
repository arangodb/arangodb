
@startDocuBlock post_admin_execute
@brief Execute a script on the server.

@RESTHEADER{POST /_admin/execute, Execute program, RestAdminExecuteHandler}

@RESTALLBODYPARAM{body,string,required}
The body to be executed.

@RESTDESCRIPTION
Executes the javascript code in the body on the server as the body
of a function with no arguments. If you have a *return* statement
then the return value you produce will be returned as content type
*application/json*. If the parameter *returnAsJSON* is set to
*true*, the result will be a JSON object describing the return value
directly, otherwise a string produced by JSON.stringify will be
returned.

Note that this API endpoint will only be present if the server was
started with the option `--javascript.allow-admin-execute true`.

The default value of this option is `false`, which disables the execution of
user-defined code and disables this API endpoint entirely.
This is also the recommended setting for production.

@RESTRETURNCODE{200}
is returned when everything went well, or if a timeout occurred. In the
latter case a body of type application/json indicating the timeout
is returned. depending on *returnAsJSON* this is a json object or a plain string.

@RESTRETURNCODE{403}
is returned if ArangoDB is not running in cluster mode.

@RESTRETURNCODE{404}
is returned if ArangoDB was not compiled for cluster operation.

@endDocuBlock


@startDocuBlock JSF_post_admin_execute
@brief Execute a script on the server.

@RESTHEADER{POST /_admin/execute, Execute program}

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
@endDocuBlock


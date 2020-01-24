
@startDocuBlock post_admin_echo
@brief Send back what was sent in, headers, post body etc.

@RESTHEADER{POST /_admin/echo, Return current request, adminEchoJs}

@RESTALLBODYPARAM{body,object,required}
The body can be any type and is simply forwarded.

@RESTDESCRIPTION
The call returns an object with the servers request information

@RESTRETURNCODES

@RESTRETURNCODE{200}
Echo was returned successfully.

@RESTREPLYBODY{authorized,boolean,required,}
whether the session is authorized

@RESTREPLYBODY{user,string,required,}
the currently user that sent this request

@RESTREPLYBODY{database,string,required,}
the database this request was executed on

@RESTREPLYBODY{url,string,required,}
the raw request URL

@RESTREPLYBODY{protocol,string,required,}
the transport, one of ['http', 'https', 'velocystream']

@RESTREPLYBODY{server,object,required,admin_echo_server_struct}

@RESTSTRUCT{address,admin_echo_server_struct,string,required,}
the bind address of the endpoint this request was sent to

@RESTSTRUCT{port,admin_echo_server_struct,integer,required,}
the port this request was sent to

@RESTREPLYBODY{client,object,required,admin_echo_client_struct}
attributes of the client connection

@RESTSTRUCT{address,admin_echo_server_struct,integer,required,}
the ip address of the client

@RESTSTRUCT{port,admin_echo_server_struct,integer,required,}
port of the client side of the tcp connection

@RESTSTRUCT{id,admin_echo_server_struct,string,required,}
a server generated id

@RESTREPLYBODY{internals,object,required,}
contents of the server internals struct

@RESTREPLYBODY{prefix,object,required,}
prefix of the database

@RESTREPLYBODY{headers,object,required,}
the list of the HTTP headers you sent

@RESTREPLYBODY{requestType,string,required,}
In this case *POST*, if you use another HTTP-Verb, you will se that (GET/DELETE, ...)

@RESTREPLYBODY{requestBody,string,required,}
stringified version of the POST body we sent

@RESTREPLYBODY{parameters,object,required,}
Object containing the query parameters

@RESTREPLYBODY{cookies,object,required,}
list of the cookies you sent

@RESTREPLYBODY{suffix,array,required,}

@RESTREPLYBODY{rawSuffix,array,required,}

@RESTREPLYBODY{path,string,required,}
relative path of this request

@RESTREPLYBODY{rawRequestBody,array,required,}
List of digits of the sent characters

@endDocuBlock

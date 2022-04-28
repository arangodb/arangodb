
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
Whether the session is authorized

@RESTREPLYBODY{user,string,required,}
The name of the current user that sent this request

@RESTREPLYBODY{isAdminUser,boolean,required,}
Whether the current user is an administrator

@RESTREPLYBODY{database,string,required,}
The name of the database this request was executed on

@RESTREPLYBODY{url,string,required,}
The raw request URL

@RESTREPLYBODY{protocol,string,required,}
The transport protocol, one of `"http"`, `"https"`, `"velocystream"`

@RESTREPLYBODY{portType,string,required,}
The type of the socket, one of `"tcp/ip"`, `"unix"`, `"unknown"`

@RESTREPLYBODY{server,object,required,admin_echo_server_struct}
Attributes of the server connection

@RESTSTRUCT{address,admin_echo_server_struct,string,required,}
The bind address of the endpoint this request was sent to

@RESTSTRUCT{port,admin_echo_server_struct,integer,required,}
The port this request was sent to

@RESTSTRUCT{endpoint,admin_echo_server_struct,string,required,}
The endpoint this request was sent to

@RESTREPLYBODY{client,object,required,admin_echo_client_struct}
Attributes of the client connection

@RESTSTRUCT{address,admin_echo_client_struct,integer,required,}
The IP address of the client

@RESTSTRUCT{port,admin_echo_client_struct,integer,required,}
The port of the TCP connection on the client-side

@RESTSTRUCT{id,admin_echo_client_struct,string,required,}
A server generated ID

@RESTREPLYBODY{internals,object,required,}
Contents of the server internals struct

@RESTREPLYBODY{prefix,object,required,}
The prefix of the database

@RESTREPLYBODY{headers,object,required,}
The list of the HTTP headers you sent

@RESTREPLYBODY{requestType,string,required,}
The HTTP method that was used for the request (`"POST"`). The endpoint can be
queried using other verbs, too (`"GET"`, `"PUT"`, `"PATCH"`, `"DELETE"`).

@RESTREPLYBODY{requestBody,string,required,}
Stringified version of the request body you sent

@RESTREPLYBODY{rawRequestBody,object,required,}
The sent payload as a JSON-encoded Buffer object

@RESTREPLYBODY{parameters,object,required,}
An object containing the query parameters

@RESTREPLYBODY{cookies,object,required,}
A list of the cookies you sent

@RESTREPLYBODY{suffix,array,required,}
A list of the decoded URL path suffixes. You can query the endpoint with
arbitrary suffixes, e.g. `/_admin/echo/foo/123`

@RESTREPLYBODY{rawSuffix,array,required,}
A list of the percent-encoded URL path suffixes

@RESTREPLYBODY{path,string,required,}
The relative path of this request (decoded, excluding `/_admin/echo`)


@endDocuBlock

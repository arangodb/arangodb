@startDocuBlock get_api_cluster_endpoints
@brief This API call returns information about all Coordinator endpoints (cluster only).

@RESTHEADER{GET /_api/cluster/endpoints, Get information about all Coordinator endpoints, handleCommandEndpoints:listClusterEndpoints}

@RESTDESCRIPTION
Returns an object with an attribute `endpoints`, which contains an
array of objects, which each have the attribute `endpoint`, whose value
is a string with the endpoint description. There is an entry for each
Coordinator in the cluster. This method only works on Coordinators in
cluster mode. In case of an error the `error` attribute is set to
`true`.

@RESTRETURNCODES

@RESTRETURNCODE{200} is returned when everything went well.

@RESTREPLYBODY{error,boolean,required,}
boolean flag to indicate whether an error occurred (*true* in this case)

@RESTREPLYBODY{code,integer,required,int64}
the HTTP status code - 200

@RESTREPLYBODY{endpoints,array,required,cluster_endpoints_struct}
A list of active cluster endpoints.

@RESTSTRUCT{endpoint,cluster_endpoints_struct,string,required,}
The bind of the Coordinator, like `tcp://[::1]:8530`

@RESTRETURNCODE{501} server is not a Coordinator or method was not GET.

@endDocuBlock

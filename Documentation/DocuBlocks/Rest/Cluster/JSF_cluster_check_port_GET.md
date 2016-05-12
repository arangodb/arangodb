
@startDocuBlock JSF_cluster_check_port_GET
@brief allows to check whether a given port is usable

@RESTHEADER{GET /_admin/clusterCheckPort, Check port}

@RESTQUERYPARAMETERS

@RESTQUERYPARAM{port,integer,required}

@RESTDESCRIPTION Checks whether the requested port is usable.

@RESTRETURNCODES

@RESTRETURNCODE{200} is returned when everything went well.

@RESTRETURNCODE{400} the parameter port was not given or is no integer.
@endDocuBlock


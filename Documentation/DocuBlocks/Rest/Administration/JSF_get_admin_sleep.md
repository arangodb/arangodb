
@startDocuBlock JSF_get_admin_sleep
@brief Suspend the execution for a specified duration before returnig

@RESTHEADER{GET /_admin/sleep, Sleep for a specified amount of seconds}

@RESTQUERYPARAMETERS

@RESTURLPARAM{duration,integer,required}
wait `duration` seconds until the reply is sent.

@RESTDESCRIPTION

The call returns an object with the attribute *duration*. This takes
as many seconds as the duration argument says.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Sleep was conducted successfully.
@endDocuBlock


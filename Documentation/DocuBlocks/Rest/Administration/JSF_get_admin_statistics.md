
@startDocuBlock JSF_get_admin_statistics
@brief return the statistics information

@RESTHEADER{GET /_admin/statistics, Read the statistics}

@RESTDESCRIPTION

Returns the statistics information. The returned object contains the
statistics figures grouped together according to the description returned by
*_admin/statistics-description*. For instance, to access a figure *userTime*
from the group *system*, you first select the sub-object describing the
group stored in *system* and in that sub-object the value for *userTime* is
stored in the attribute of the same name.

In case of a distribution, the returned object contains the total count in
*count* and the distribution list in *counts*. The sum (or total) of the
individual values is returned in *sum*.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Statistics were returned successfully.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestAdminStatistics1}
    var url = "/_admin/statistics";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock


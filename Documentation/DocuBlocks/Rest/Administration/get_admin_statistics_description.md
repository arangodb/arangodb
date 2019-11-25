
@startDocuBlock get_admin_statistics_description
@brief fetch descriptive info of statistics

@RESTHEADER{GET /_admin/statistics-description, Statistics description, getStatisticsDescription}

@RESTDESCRIPTION
Returns a description of the statistics returned by */_admin/statistics*.
The returned objects contains an array of statistics groups in the attribute
*groups* and an array of statistics figures in the attribute *figures*.

A statistics group is described by

- *group*: The identifier of the group.
- *name*: The name of the group.
- *description*: A description of the group.

A statistics figure is described by

- *group*: The identifier of the group to which this figure belongs.
- *identifier*: The identifier of the figure. It is unique within the group.
- *name*: The name of the figure.
- *description*: A description of the figure.
- *type*: Either *current*, *accumulated*, or *distribution*.
- *cuts*: The distribution vector.
- *units*: Units in which the figure is measured.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Description was returned successfully.

@RESTREPLYBODY{groups,array,required,admin_statistics_group_struct}
A statistics group

@RESTSTRUCT{group,admin_statistics_group_struct,string,required,}
The identifier of the group.

@RESTSTRUCT{name,admin_statistics_group_struct,string,required,}
The name of the group.

@RESTSTRUCT{description,admin_statistics_group_struct,string,required,}
A description of the group.

@RESTREPLYBODY{figures,array,required,admin_statistics_figures_struct}
A statistics figure

@RESTSTRUCT{group,admin_statistics_figures_struct,string,required,}
The identifier of the group to which this figure belongs.

@RESTSTRUCT{identifier,admin_statistics_figures_struct,string,required,}
The identifier of the figure. It is unique within the group.

@RESTSTRUCT{name,admin_statistics_figures_struct,string,required,}
The name of the figure.

@RESTSTRUCT{description,admin_statistics_figures_struct,string,required,}
A description of the figure.

@RESTSTRUCT{type,admin_statistics_figures_struct,string,required,}
Either *current*, *accumulated*, or *distribution*.

@RESTSTRUCT{cuts,admin_statistics_figures_struct,string,required,}
The distribution vector.

@RESTSTRUCT{units,admin_statistics_figures_struct,string,required,}
Units in which the figure is measured.

@RESTREPLYBODY{code,integer,required,int64}
the HTTP status code

@RESTREPLYBODY{error,boolean,required,}
the error, *false* in this case

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestAdminStatisticsDescription1}
    var url = "/_admin/statistics-description";
    var response = logCurlRequest('GET', url);

    assert(response.code === 200);

    logJsonResponse(response);
@END_EXAMPLE_ARANGOSH_RUN
@endDocuBlock

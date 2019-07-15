@startDocuBlock post_admin_backup_list
@brief list all local backups

@RESTHEADER{POST /_admin/backup/list, List backups}

@RESTDESCRIPTION
Lists all locally found backups.

@RESTBODYPARAM{id,string,optional,string}
The body can either be empty (in which case all available backups are
listed), or it can be an object with an attribute `id`, which 
is a string. In the latter case the returned list
is restricted to the backup with the given id.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If all is well, code 200 is returned.

@RESTRETURNCODE{400}
If the list command is invoked with bad parameters, then an *HTTP 400*
is returned.

@RESTRETURNCODE{404}
If an `id` or a list of ids was given and the given ids were not found
as identifiers of a backup, an *HTTP 404 NOT FOUND* is returned.

@RESTRETURNCODE{405}
If the list command is invoked with any HTTP
method other than `POST`, then an *HTTP 405 METHOD NOT ALLOWED* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestBackupListBackup}
    var backup = internal.arango.POST("/_admin/backup/create","");
    var backup2 = internal.arango.POST("/_admin/backup/create","");
    var url = "/_admin/backup/list";
    var body = {};

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
    body = {
      result: {
        list: {
            "2019-04-28T12.00.00Z_my-label": {
                "id": "2019-04-28T12.00.00Z_my-label",
                "version": "3.4.5",
                "datetime": "2019-04-28T12.00.00Z"
            },
            "2019-04-28T12.10.00Z-other-label": {
                "id": "2019-04-28T12.10.00Z-other-label",
                "version": "3.4.5",
                "datetime": "2019-04-28T12.10.00Z"
            }
        }
      }
    };
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock

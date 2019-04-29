@startDocuBlock post_admin_backup_list
@brief list all backups or a specific one

@RESTHEADER{POST /_admin/backup/list, List backups}

@RESTDESCRIPTION

Lists all backups or a specific one. If the action is performed with
an empty body, all available past backups are listed. The request may,
however, contain an object with the following attribute to list a
specific past backup:

@RESTBODYPARAM{id,string,optional,string}
The backup identifier of a specific backup

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the create command is invoced with bad parameters or any HTTP
method other than `POST`, then a *HTTP 400* is returned.

@RESTRETURNCODE{404}
If the list command was invoced with a specific backup identifier,
which could not be found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestBackupListBackup}
    var url = "/_api/backup/list";
    var body = {};

    var reponse = logCurlRequest('POST', url, body);
    
    assert(response.code === 200);
    
    logJSONResponse(response);
    body = {
      result: {
        id: ["2019-04-28T12.00.00Z", "2019-04-28T12.10.00Z"]
      }
    };
@END_EXAMPLE_ARANGOSH_RUN

@EXAMPLE_ARANGOSH_RUN{RestBackupListOneBackup}
    var url = "/_api/backup/list";
    var body = {
      "id": "2019-04-28T12.00.00Z"
    };

    var reponse = logCurlRequest('POST', url, body);
    
    assert(response.code === 200);
    
    logJSONResponse(response);
    body = {
      result: {
        id: ["2019-04-28T12.00.00Z", "2019-04-28T12.10.00Z"]
      }
    };
@END_EXAMPLE_ARANGOSH_RUN



@endDocuBlock

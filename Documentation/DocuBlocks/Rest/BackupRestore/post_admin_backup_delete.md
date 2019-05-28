@startDocuBlock post_admin_backup_delete
@brief delete a specific local backup

@RESTHEADER{POST /_admin/backup/delete, Delete a hot backup}

@RESTDESCRIPTION

Delete a specific local backup identified by 

@RESTBODYPARAM{id,string,required,string}
The identifier for this backup. 

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the create command is invoced with bad parameters or any HTTP
method other than `POST`, then a *HTTP 400* is returned.

@RESTRETURNCODE{404}
If a backup corresponding to the identifier, `id`,  cannot be found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestBackupListBackup}
    var url = "/_api/backup/delete";
    var body = {"id" : "2019-05-01T00.00.00Z_some-label"};

    var reponse = logCurlRequest('POST', url, body);
    
    assert(response.code === 200);
    
    logJSONResponse(response);
    body = {
      result: {
        id: "2019-05-01T00.00.00Z_some-label"
      }
    };
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock

@startDocuBlock post_admin_backup_list
@brief all local backups

@RESTHEADER{POST /_admin/backup/list, List backups}

@RESTDESCRIPTION

Lists all locally found backups. 

@RESTRETURNCODES

@RESTRETURNCODE{400}
If the create command is invoced with bad parameters or any HTTP
method other than `POST`, then a *HTTP 400* is returned.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestBackupListBackup}
    var url = "/_api/backup/list";
    var body = {};

    var reponse = logCurlRequest('POST', url, body);
    
    assert(response.code === 200);
    
    logJSONResponse(response);
    body = {
      result: {
        id: ["2019-04-28T12.00.00Z_my-label", "2019-04-28T12.10.00Z-other-label"]
      }
    };
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock

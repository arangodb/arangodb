@startDocuBlock post_admin_backup_restore
@brief restores from a local backup

@RESTHEADER{POST /_admin/backup/restore, Restore backup}

@RESTDESCRIPTION
Restores a consistent backup from a
snapshot in time, with a given id.

The request may contain an object with the following attributes:

@RESTBODYPARAM{id,string,required,string}
The id of the backup to restore from.

@RESTRETURNCODES

@RESTRETURNCODE{200}
Is returned if the backup could be restored.

@RESTRETURNCODE{400}
If the restore command is invoked with bad parameters or any HTTP
method other than `POST`, then a *HTTP 400* is returned. The specifics
are detailed in the returned error document.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestBackupRestoreBackup}
    var url = "/_api/backup/restore";
    var body = {
      id: "2019-07-05T09.16.43Z_abc"
    };

    var reponse = logCurlRequest('POST', url, body);

    assert(response.code === 200);

    logJSONResponse(response);
    body = {
      error: false, code: 200, 
      result: {
        result: {"previous":"FAILSAFE","isCluster":true}⏎
      }
    };
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock

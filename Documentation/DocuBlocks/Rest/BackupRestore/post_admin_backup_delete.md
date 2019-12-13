@startDocuBlock post_admin_backup_delete
@brief delete a specific local backup

@RESTHEADER{POST /_admin/backup/delete, Delete a backup}

@RESTDESCRIPTION
Delete a specific local backup identified by the given `id`.

@RESTBODYPARAM{id,string,required,string}
The identifier for this backup.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If all is well, this code 200 is returned.

@RESTRETURNCODE{400}
If the delete command is invoked with bad parameters or any HTTP
method other than `POST`, then an *HTTP 400* is returned.

@RESTRETURNCODE{404}
If a backup corresponding to the identifier `id` cannot be found.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestBackupDeleteBackup_rocksdb}
    var backup = require("@arangodb/hotbackup").create();
    var url = "/_admin/backup/delete";
    var body = {"id" : backup.id};

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
    body = {
      result: {
      }
    };
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock

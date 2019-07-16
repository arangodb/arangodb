@startDocuBlock post_admin_backup_upload
@brief upload a specific local backup

@RESTHEADER{POST /_admin/backup/upload, Upload a backup to a remote repository}

@RESTDESCRIPTION
Upload a specific local backup to a remote repository, or query
progress on a previously scheduled upload operation.

@RESTBODYPARAM{id,string,optional,string}
The identifier for this backup. This is required when an upload
operation is scheduled. In this case leave out the `uploadId`
attribute.

@RESTBODYPARAM{remoteRepository,string,optional,string}
URL of remote reporsitory. This is required when an upload
operation is scheduled. In this case leave out the `uploadId`
attribute.

@RESTBODYPARAM{config,object,optional,object}
Configuration of remote repository. This is required when an upload
operation is scheduled. In this case leave out the `uploadId`
attribute.

@RESTBODYPARAM{uploadId,string,optional,string}
Upload ID to specify for which upload operation progress is queried.
If you specify this, leave out all other body parameters.

@RESTRETURNCODES

@RESTRETURNCODE{200}
If all is well, code 200 is returned if progress is inquired or the
operation is aborted.

@RESTRETURNCODE{202}
If all is well, code 202 is returned if a new operation is scheduled.

@RESTRETURNCODE{400}
If the upload command is invoced with bad parameters or any HTTP
method other than `POST`, then an *HTTP 400* is returned.

@RESTRETURNCODE{401}
If the authentication to the rempote repository fails, then an *HTTP
400* is returned.

@RESTRETURNCODE{404}
If a backup corresponding to the identifier `id`  cannot be found, or if
there is no known upload operation with the given `uploadId`.

@EXAMPLES

@EXAMPLE_ARANGOSH_RUN{RestBackupUploadBackup_rocksdb}
    try {
      require("fs").makeDirectory("/tmp/backups");
    } catch(e) {
    }
    var backup = require("@arangodb/hotbackup").create();
    var url = "/_admin/backup/upload";
    var body = {"id" : backup.id,
                "remoteRepository": "local://tmp/backups",
                "config": {
                  "local": {
                    "type":"local"
                  }
                }
               };
    var response = logCurlRequest('POST', url, body);

    assert(response.code === 202);

    logJsonResponse(response);
    body = {
      result: {
        uploadId: "10046"
      }
    };
@END_EXAMPLE_ARANGOSH_RUN

@EXAMPLE_ARANGOSH_RUN{RestBackupUploadBackupStarted_rocksdb}
    try {
      require("fs").makeDirectory("/tmp/backups");
    } catch(e) {
    }
    var backup = internal.arango.POST("/_admin/backup/create","");
    var body = {"id" : backup.result.id,
                "remoteRepository": "local://tmp/backups",
                "config": {
                  "local": {
                    "type":"local"
                  }
                }
               };
    var upload = internal.arango.POST("/_admin/backup/upload",body);
    var url = "/_admin/backup/upload";
    var body = {"uploadId" : upload.result.uploadId};

    var response = logCurlRequest('POST', url, body);

    assert(response.code === 200);

    logJsonResponse(response);
    body = {
      "result": {
        "Timestamp": "2019-05-14T14:50:56Z",
        "BackupId": "2019-05-01T00.00.00Z_some-label",
        "DBServers": {
          "SNGL": {
            "Status": "COMPLETED"
          }
        }
      }
    };
@END_EXAMPLE_ARANGOSH_RUN

@endDocuBlock
